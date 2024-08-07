#include <causenet/causenet.hpp>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
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

using offset_t = std::uint64_t;

struct __attribute__((packed)) EdgeEntry {
	uint32_t targetIdx;
	uint32_t numSupport;
	offset_t supportOffset;

	inline const char* support(const CausenetFile* base) const {
		return reinterpret_cast<const char*>(base) + supportOffset;
	}
};
static_assert(sizeof(EdgeEntry) == 16);

static const EdgeEntry nulledge = {.targetIdx = (uint32_t)-1, .numSupport = 0, .supportOffset = 0};

struct __attribute__((packed)) NodeEntry {
	offset_t nameOffset;
	offset_t effectOffset;

	inline const char* name(const void* base) const { return reinterpret_cast<const char*>(base) + nameOffset; }
	inline const EdgeEntry* effects(const void* base) const {
		return reinterpret_cast<const EdgeEntry*>(reinterpret_cast<const char*>(base) + effectOffset);
	}
};
static_assert(sizeof(NodeEntry) == 16);

struct __attribute__((packed)) causenet::CausenetFile {
public:
	const size_t numNodes;
	const NodeEntry nodes[0];

	inline const char* getCauseName(size_t i) const noexcept { return nodes[i].name(this); }
	inline const EdgeEntry* getFirstNeighbor(size_t i) const noexcept { return nodes[i].effects(this); }
};
static_assert(sizeof(CausenetFile) == 8);

static const CausenetFile& mmapFile(int fd, size_t size) {
	auto filemapped = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
	return *reinterpret_cast<const CausenetFile*>(filemapped);
}

Causenet::Causenet(const std::filesystem::path& path)
		: fd(open64(path.c_str(), O_RDONLY)), file(mmapFile(fd, fs::file_size(path))) {}

size_t Causenet::getConceptIdx(std::string name) const noexcept {
	for (size_t i = 0; i < file.numNodes; ++i)
		if (name == getConceptByIdx(i))
			return i;
	return -1;
}
const std::string Causenet::getConceptByIdx(size_t idx) const noexcept { return std::string(file.getCauseName(idx)); }
Generator<std::string> Causenet::getConcepts() const noexcept {
	for (size_t i = 0; i < file.numNodes; ++i)
		co_yield getConceptByIdx(i);
}
size_t Causenet::numConcepts() const noexcept { return file.numNodes; }
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
			const char* data = n->support(&file);
			for (size_t i = 0; i < n->numSupport; ++i) {
				Support support;
				support.sourceTypeId = *reinterpret_cast<const SourceType*>(data);
				data += sizeof(support.sourceTypeId);
				support.id = std::string(data);
				data += support.id.length() + 1;
				support.content = std::string(data);
				data += support.content.length() + 1;
				supports.emplace_back(std::move(support));
			}
			return supports;
		}
	}
	return {};
}

template <typename K, typename V>
static std::pair<V&, bool> insertOrGet(std::map<K, V>& map, const K& key, V&& newVal) {
	auto it = map.lower_bound(key);
	if ((it != map.end()) && !(map.key_comp()(key, it->first))) {
		return {it->second, false};
	} else {
		it = map.insert(it, {key, newVal});
		return {it->second, true};
	}
}
struct JSONNode {
	std::string name;
	std::map<size_t, std::vector<Support>> effects;
};

static void extractConcepts(std::istream& file, std::vector<JSONNode>& nodes) {
	std::map<std::string, size_t> conceptToIdx;
	const int numRows = 11'609'890; // Yay, hardcoded to print progress
	int i = 0;
	for (std::string line; std::getline(file, line);) {
		json::Document doc;
		doc.Parse(line.c_str());
		auto& data = doc["causal_relation"];
		std::string cause = data["cause"]["concept"].GetString();
		std::string effect = data["effect"]["concept"].GetString();
		auto [causeIdx, cInserted] = insertOrGet(conceptToIdx, cause, nodes.size());
		if (cInserted)
			nodes.push_back({cause});
		auto [effectIdx, eInserted] = insertOrGet(conceptToIdx, effect, nodes.size());
		if (eInserted)
			nodes.push_back({effect});
		auto& sources = doc["sources"];
		auto& supports = nodes[causeIdx].effects[effectIdx];
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
				support.id = payload["clueweb12_page_id"].GetString();
			} else {
				throw std::runtime_error(std::format("Invalid source type: {}", source["type"].GetString()));
			}
			supports.emplace_back(std::move(support));
		}
		++i;
		if (i % 100 == 0)
			std::cout << "\r" << (i * 100.0f / numRows) << "% \r";
	}
	std::cout << "Num Concepts: " << conceptToIdx.size() << std::endl;
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
void Causenet::jsonlToBinary(const fs::path& inJsonl, fs::path& outBinary) {
	std::vector<JSONNode> nodes;
	{
		std::ifstream infile(inJsonl);
		assert(infile);
		extractConcepts(infile, nodes);
	}

	// Exporting into the binary format:
	std::ofstream outfile(outBinary, std::ios::binary);
	size_t numNodes = nodes.size();
	const offset_t endOfHeader = sizeof(CausenetFile);
	const offset_t endOfNodeIndex = endOfHeader + numNodes * sizeof(NodeEntry);
	assert(outfile.tellp() == 0);
	{ // Write Header
		std::cout << "Writing Header..." << std::endl;
		auto tmp = numNodes;
		outfile.write(reinterpret_cast<const char*>(&tmp), sizeof(tmp)); // Write number of concepts
		// Write the offset to their name for each concept
	}
	assert(outfile.tellp() == endOfHeader);
	offset_t offset = endOfNodeIndex;
	{ // Write Node Index
		std::cout << "Writing Node Index..." << std::endl;
		for (const auto& node : nodes) {
			NodeEntry entry{.nameOffset = offset, .effectOffset = offset += node.name.length() + 1};
			outfile.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
			offset += (node.effects.size() + 1) * sizeof(EdgeEntry);
			// "+ 1" because the null-edge marks the end of the edge list
		}
	}
	assert(outfile.tellp() == endOfNodeIndex);
	const offset_t endOfNodeEntries = offset;
	// Here, Offset points to the end of the last EdgeEntry (i.e., the start of the support datablock)
	{ // Write the concepts' names and adjacency information
		std::cout << "Writing Node Entries..." << std::endl;
		for (const auto& node : nodes) {
			outfile.write(node.name.c_str(), node.name.length() + 1);
			for (const auto& [effect, supports] : node.effects) {
				assert(effect < numNodes);
				EdgeEntry edge = {
						.targetIdx = (uint32_t)effect, .numSupport = (uint32_t)supports.size(), .supportOffset = offset
				};
				outfile.write(reinterpret_cast<const char*>(&edge), sizeof(edge));
				for (const auto& support : supports) {
					offset += sizeof(support.sourceTypeId);
					offset += support.id.length() + 1;
					offset += support.content.length() + 1;
				}
			}
			outfile.write(reinterpret_cast<const char*>(&nulledge), sizeof(nulledge)); // Terminate with the null-edge
		}
	}
	assert(outfile.tellp() == endOfNodeEntries);
	const offset_t endOfSources = offset;

	{ // Write Sources
		std::cout << "Writing Sources..." << std::endl;
		for (const auto& node : nodes) {
			for (const auto& [_, supports] : node.effects) {
				for (const auto& support : supports) {
					outfile.write(reinterpret_cast<const char*>(&support.sourceTypeId), sizeof(support.sourceTypeId));
					outfile.write(support.id.c_str(), support.id.length() + 1);
					outfile.write(support.content.c_str(), support.content.length() + 1);
				}
			}
		}
	}
	assert(outfile.tellp() == endOfSources);
	std::cout << "Done" << std::endl;
}

Causenet Causenet::fromFile(const fs::path& path) { return Causenet(path); }