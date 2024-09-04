#include <causenet/causenet.hpp>

#include "./causenet_writer.hpp"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <vector>

// Linux only headers :(
#include <fcntl.h>
#include <sys/mman.h>

using causenet::Causenet;
using causenet::CausenetFile;
using causenet::SourceType;
using causenet::Support;
namespace json = rapidjson;
namespace fs = std::filesystem;

//
std::unordered_map<std::string, std::string> getMap() {
	std::ifstream file("rec-to-trec-id.txt");
	assert(file);
	std::regex rgx("^<(.*)>\\t(.*)$");
	std::unordered_map<std::string, std::string> ret;
	std::smatch match;
	for (std::string line; std::getline(file, line);) {
		assert(std::regex_search(line, match, rgx));
		ret[match[1].str()] = match[2].str();
	}
	return ret;
}
std::string warcId2ClueWebId(std::string id) {
	static auto map = getMap();
	return map[id];
}
//

struct __attribute__((packed)) causenet::CausenetFile {
public:
	const Header header;

	inline const size_t numNodes() const noexcept { return header.numNodes; }
	inline const NodeEntry* nodes() const noexcept { return reinterpret_cast<const NodeEntry*>(header.nodeBase()); }

	inline const char* getCauseName(size_t i) const noexcept { return nodes()[i].name(header); }
	inline const EdgeEntry* getFirstNeighbor(size_t i) const noexcept { return nodes()[i].effects(header); }
};

static const CausenetFile& mmapFile(int fd, size_t size) {
	auto filemapped = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
	return *reinterpret_cast<const CausenetFile*>(filemapped);
}

Causenet::Causenet(const std::filesystem::path& path)
		: fd(open64(path.c_str(), O_RDONLY)), file(mmapFile(fd, fs::file_size(path))) {}

size_t Causenet::getConceptIdx(std::string name) const noexcept {
	for (size_t i = 0; i < file.numNodes(); ++i)
		if (name == getConceptByIdx(i))
			return i;
	return -1;
}
const std::string Causenet::getConceptByIdx(size_t idx) const noexcept { return std::string(file.getCauseName(idx)); }
Generator<std::string> Causenet::getConcepts() const noexcept {
	for (size_t i = 0; i < file.numNodes(); ++i)
		co_yield getConceptByIdx(i);
}
size_t Causenet::numConcepts() const noexcept { return file.numNodes(); }
Generator<std::tuple<size_t, unsigned>> Causenet::getEffects(size_t conceptIdx) const noexcept {
	for (auto n = file.getFirstNeighbor(conceptIdx); n->targetIdx != nulledge.targetIdx; ++n)
		co_yield {n->targetIdx, n->numSupport};
}
size_t Causenet::numEffects(size_t conceptIdx) const noexcept {
	size_t n = 0;
	for (auto neighbors = file.getFirstNeighbor(conceptIdx); neighbors[n].targetIdx != nulledge.targetIdx; ++n)
		;
	return n;
}
std::vector<Support> Causenet::getSupport(size_t causeIdx, size_t effectIdx) const noexcept {
	for (auto n = file.getFirstNeighbor(causeIdx); n->targetIdx != nulledge.targetIdx; ++n) {
		if (effectIdx == n->targetIdx) {
			std::vector<Support> supports;
			supports.reserve(n->numSupport);
			for (auto support : n->support(file.header))
				supports.emplace_back(std::move(support));
			return supports;
		}
	}
	return {};
}

/**
 * @brief 
 * @details A CauseNet binary file created with this method has the following structure:
 * ```
 * +-----------------------+                                          \
 * | uint64_t numNodes     |                                           |  HEADER
 * +-----------------------+                                          /
 * | uint32_t nameOffset   | NodeEntry[0]           -----------+      \
 * | uint32_t effectOffset |                        --------+  |       | NODE LIST
 * +-----------------------+                                |  |       |
 * | uint32_t nameOffset   | NodeEntry[1]                   |  |       |
 * | uint32_t effectOffset |                                |  |       |
 * +-----------------------+                                |  |       .
 *          ...                                             |  |       .
 * +-----------------------+                                |  |       .
 * | uint32_t nameOffset   | NodeEntry[numNodes-1]          |  |       |
 * | uint32_t effectOffset |                                |  |      /
 * +-----------------------+                                |  |      \
 * | Null-terminated name  |                       <--------|--+       | NODE INFO 0
 * | of NodeEntry[0]       |                                |          |
 * +-----------------------+                                |          |
 * | uint32_t targetIdx    | EdgeEntry[0] of NodeEntry[0] <-+          |
 * | uint32_t numSupport   |                                           |
 * | uint32_t supportOffset|                                           |
 * +-----------------------+                                           |
 * | uint32_t targetIdx    | EdgeEntry[1] of NodeEntry[0]              |
 * | uint32_t numSupport   |                                           |
 * | uint32_t supportOffset|                                           |
 * +-----------------------+                                           |
 *          ...                                                        |
 * +-----------------------+                                           |
 * | uint32_t -1           | Nulledge of NodeEntry[0]                  |
 * | uint32_t 0            |                                           |
 * | uint32_t 0            |                                           /
 * +-----------------------+                                           \
 * | Null-terminated name  |                                           | NODE INFO 1
 * | of NodeEntry[1]       |                                           |
 * +-----------------------+                                           |
 * | uint32_t targetIdx    | EdgeEntry[0] of NodeEntry[1]              |
 * | uint32_t numSupport   |                                           |
 * | uint32_t supportOffset|                                           |
 * +-----------------------+                                           .
 *          ...                                                        .
 * +-----------------------+                                           .
 * | uint32_t -1           | Nulledge of NodeEntry[numNodes-1]         | NODE INFO numNodes-1
 * | uint32_t 0            |                                           |
 * | uint32_t 0            |                                          /
 * +-----------------------+                                          \
 * | byte     typeId       |                                           | SOURCES
 * | string   id           |                                           |
 * | string   content      |                                           |
 * +-----------------------+                                           .
 *          ...                                                        .
 * +-----------------------+                                           .
 * | byte     typeId       |                                           |
 * | string   id           |                                           |
 * | string   content      |                                           |
 * +-----------------------+                                          /
 * ```
 * 
 * @param inJsonl 
 * @param outBinary 
 */
void Causenet::jsonlToBinary(const fs::path& inJsonl, const fs::path& outBinary) {
	std::ifstream file(inJsonl);
	assert(file);
	const int numRows = 11'609'890; // Yay, hardcoded to print progress
	int i = 0;
	internal::CausenetWriter writer(outBinary);
	for (std::string line; std::getline(file, line);) {
		json::Document doc;
		doc.Parse(line.c_str());
		auto& data = doc["causal_relation"];
		std::string cause = data["cause"]["concept"].GetString();
		std::string effect = data["effect"]["concept"].GetString();
		auto& sources = doc["sources"];
		std::vector<Support> supports;
		for (auto it = sources.Begin(); it != sources.End(); ++it) {
			auto& source = *it;
			auto& payload = source["payload"];
			auto sentence = payload.HasMember("sentence") ? payload["sentence"].GetString() : "";
			Support support{.content = std::move(sentence)};
			if (source["type"] == "wikipedia_infobox") {
				support.sourceTypeId = SourceType::WikipediaInfobox;
				support.id = payload["wikipedia_revision_id"].GetString();
			} else if (source["type"] == "wikipedia_list") {
				support.sourceTypeId = SourceType::WikipediaList;
				support.id = payload["wikipedia_revision_id"].GetString();
			} else if (source["type"] == "wikipedia_sentence") {
				support.sourceTypeId = SourceType::WikipediaSentence;
				support.id = payload["wikipedia_revision_id"].GetString();
			} else if (source["type"] == "clueweb12_sentence") {
				support.sourceTypeId = SourceType::ClueWeb12Sentence;
				support.id = warcId2ClueWebId(payload["clueweb12_page_id"].GetString());
			} else {
				throw std::runtime_error(std::format("Invalid source type: {}", source["type"].GetString()));
			}
			supports.emplace_back(support);
		}
		writer.writeEdge(cause, effect, supports);
		++i;
		if (i % 100 == 0)
			std::cout << "\r" << (i * 100.0f / numRows) << "% \r";
		//if (i >= 1000000) /** \todo remove */
		//	break;
	}
}

Causenet Causenet::fromFile(const fs::path& path) { return Causenet(path); }