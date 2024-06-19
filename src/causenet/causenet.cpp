#include <causenet/causenet.hpp>

#include <rapidjson/document.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

// Linux only headers :(
#include <fcntl.h>
#include <sys/mman.h>

using causenet::Causenet;
using causenet::CausenetFile;
namespace json = rapidjson;
namespace fs = std::filesystem;

struct __attribute__((packed)) EdgeEntry {
	uint32_t targetIdx;
	uint32_t weight;
};
static_assert(sizeof(EdgeEntry) == 8);

struct __attribute__((packed)) causenet::CausenetFile {
public:
	const size_t numNodes;
	const uint32_t offsets[0];

	inline const char* getCauseName(size_t i) const noexcept {
		return reinterpret_cast<const char*>(this) + offsets[2 * i];
	}

	inline const EdgeEntry* getFirstNeighbor(size_t i) const noexcept {
		return reinterpret_cast<const EdgeEntry*>(reinterpret_cast<const char*>(this) + offsets[2 * i + 1]);
	}
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
	for (auto n = file.getFirstNeighbor(conceptIdx); n->targetIdx != (uint32_t)-1; ++n)
		co_yield {n->targetIdx, n->weight};
}
size_t Causenet::numEffects(size_t conceptIdx) const noexcept {
	size_t n = 0;
	for (auto neighbors = file.getFirstNeighbor(conceptIdx); neighbors[n].targetIdx != (uint32_t)-1; ++n)
		;
	return n;
}

void Causenet::jsonlToBinary(const fs::path& inJsonl, fs::path& outBinary) {
	std::set<std::string> nodes;
	std::map<std::string, std::map<std::string, uint32_t>> numEdges;
	{
		// Read the structure information
		std::ifstream infile(inJsonl);
		assert(infile);
		std::string line;
		//int limit = 100000;
		int numRows = /*std::min(*/ 11'609'890; //, limit);
		int i = 0;
		while (std::getline(infile, line)) {
			json::Document doc;
			doc.Parse(line.c_str());
			auto& data = doc["causal_relation"];
			auto cause = data["cause"]["concept"].GetString();
			auto effect = data["effect"]["concept"].GetString();
			auto [itc, _1] = nodes.emplace(cause);
			auto [ite, _2] = nodes.emplace(effect);
			numEdges[*itc][*ite]++;
			++i;
			if (i % 100 == 0)
				std::cout << "\r" << (i * 100.0f / numRows) << "% \r";
			//if (i >= limit)
			//	break;
		}
		std::cout << "Num Concepts: " << nodes.size() << std::endl;
	}

	// Exporting into the binary format:
	std::map<std::string, size_t> conceptToIdx;
	std::ofstream outfile(outBinary, std::ios::binary);
	auto tmp = nodes.size();
	outfile.write(reinterpret_cast<const char*>(&tmp), sizeof(tmp)); // Write number of concepts
																	 // Write the offset to their name for each concept
	uint32_t offset = sizeof(tmp) + 2 * nodes.size() * sizeof(uint32_t);
	for (auto&& node : nodes) {
		conceptToIdx[node] = conceptToIdx.size();
		outfile.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
		offset += node.length() + 1;
		outfile.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
		offset += (numEdges[node].size() + 1) * 2 * sizeof(uint32_t);
	}
	// Write the concepts' names and adjacency information
	for (auto&& node :
		 nodes) { // We iterated the nodes here instead of the map to ensure the same order as in the previous loop
		outfile.write(node.c_str(), node.length() + 1);
		EdgeEntry tmp;
		for (auto&& [effect, num] : numEdges[node]) {
			tmp = {.targetIdx = (uint32_t)conceptToIdx[effect], .weight = num};
			outfile.write(reinterpret_cast<const char*>(&tmp), sizeof(tmp));
		}
		numEdges.erase(node);
		tmp = {.targetIdx = (uint32_t)-1, .weight = 0};
		outfile.write(reinterpret_cast<const char*>(&tmp), sizeof(tmp));
	}
}

Causenet Causenet::fromFile(const fs::path& path) { return Causenet(path); }