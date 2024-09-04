#ifndef CAUSENET_CAUSENETWRITER_HPP
#define CAUSENET_CAUSENETWRITER_HPP

#include "./causenet_file.hpp"
#include <causenet/support.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

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

template <typename K, typename V>
static std::pair<V&, bool> insertOrGet(std::unordered_map<K, V>& map, const K& key, V&& newVal) {
	auto [it, inserted] = map.insert({key, newVal});
	return {it->second, inserted};
}

template <>
struct std::hash<causenet::Support> {
	std::size_t operator()(const causenet::Support& s) const {
		return ((std::hash<std::uint8_t>()(static_cast<std::uint8_t>(s.sourceTypeId)) ^
				 (std::hash<std::string>()(s.id) << 1)) >>
				1) ^
			   (std::hash<std::string>()(s.content) << 1);
	}
};

namespace causenet::internal {
	class CausenetWriter final {
	private:
		std::filesystem::path outfile;
		std::filesystem::path tmpfolder;
		std::fstream nodesFile;
		std::fstream nodeInfoFile;
		std::fstream sourcesFile;

		std::unordered_map<std::string, size_t> conceptToIdx;
		std::unordered_map<Support, size_t> support2Offset;
		struct JSONNode {
			std::string name;
			std::map<size_t, std::vector<offset_t>> effects;
		};
		std::vector<JSONNode> nodes;

		void writeNodeWithInfo(const JSONNode& node) {
			size_t infoOffset = writeNodeInfo(node);
			NodeEntry entry{.nameOffset = infoOffset, .effectOffset = infoOffset += node.name.length() + 1};
			nodesFile.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
		}

		size_t writeNodeInfo(const JSONNode& node) {
			size_t offset = nodeInfoFile.tellp();
			size_t supportOffset = offset + node.name.length() + 1 + (node.effects.size() + 1) * sizeof(EdgeEntry);
			nodeInfoFile.write(node.name.c_str(), node.name.length() + 1);
			for (const auto& [effect, supports] : node.effects) {
				EdgeEntry edge = {
						.targetIdx = (uint32_t)effect,
						.numSupport = (uint32_t)supports.size(),
						.supportOffset = supportOffset
				};
				nodeInfoFile.write(reinterpret_cast<const char*>(&edge), sizeof(edge));
				supportOffset += supports.size() * sizeof(decltype(supports)::value_type);
			}
			// Terminate with the null-edge
			nodeInfoFile.write(reinterpret_cast<const char*>(&nulledge), sizeof(nulledge));
			// Write list of support offsets
			for (const auto& [effect, supports] : node.effects) {
				for (const auto& supoffset : supports)
					nodeInfoFile.write(reinterpret_cast<const char*>(&supoffset), sizeof(supoffset));
			}
			return offset;
		}

		size_t writeSupport(const Support& support) {
			auto [offset, inserted] = insertOrGet(support2Offset, support, (size_t)sourcesFile.tellp());
			if (inserted) {
				sourcesFile.write(reinterpret_cast<const char*>(&support.sourceTypeId), sizeof(support.sourceTypeId));
				sourcesFile.write(support.id.c_str(), support.id.length() + 1);
				sourcesFile.write(support.content.c_str(), support.content.length() + 1);
			}
			return offset;
		}

		void writeOutfile() {
			std::ofstream out(outfile, std::ios::binary | std::ios::trunc | std::ios::in | std::ios::out);
			assert(out);
			Header header{
					.numNodes = conceptToIdx.size(),
					.conceptOffset = sizeof(Header),
					.infoOffset = sizeof(Header) + nodesFile.tellp(),
					.supportOffset = sizeof(Header) + nodesFile.tellp() + nodeInfoFile.tellp()
			};
			out.write(reinterpret_cast<const char*>(&header), sizeof(header));
			assert(out.tellp() == header.conceptOffset);
			nodesFile.seekg(0, std::ios::beg);
			out << nodesFile.rdbuf();
			assert(out.tellp() == header.infoOffset);
			nodeInfoFile.seekg(0, std::ios::beg);
			out << nodeInfoFile.rdbuf();
			assert(out.tellp() == header.supportOffset);
			sourcesFile.seekg(0, std::ios::beg);
			out << sourcesFile.rdbuf();
		}

	public:
		explicit CausenetWriter(std::filesystem::path outfile) noexcept
				: CausenetWriter(outfile, outfile.parent_path()) {}
		CausenetWriter(std::filesystem::path outfile, std::filesystem::path tmpfolder) noexcept
				: outfile(outfile), tmpfolder(tmpfolder),
				  nodesFile(tmpfolder / "nodes.tmp", std::ios::binary | std::ios::trunc | std::ios::in | std::ios::out),
				  nodeInfoFile(
						  tmpfolder / "nodeinfo.tmp", std::ios::binary | std::ios::trunc | std::ios::in | std::ios::out
				  ),
				  sourcesFile(
						  tmpfolder / "sources.tmp", std::ios::binary | std::ios::trunc | std::ios::in | std::ios::out
				  ) {
			assert(nodesFile);
			assert(nodeInfoFile);
			assert(sourcesFile);
		}

		~CausenetWriter() { close(); }

		void close() {
			std::cout << "Num Concepts: " << conceptToIdx.size() << std::endl;
			std::cout << "Num Supports: " << support2Offset.size() << std::endl;
			for (auto&& node : nodes)
				writeNodeWithInfo(node);
			writeOutfile();
		}

		void writeEdge(const std::string& from, const std::string& to, std::vector<Support> supports) {
			auto [causeIdx, cInserted] = insertOrGet(conceptToIdx, from, nodes.size());
			if (cInserted)
				nodes.push_back({from});
			auto [effectIdx, eInserted] = insertOrGet(conceptToIdx, to, nodes.size());
			if (eInserted)
				nodes.push_back({to});
			auto& supportOffsets = nodes[causeIdx].effects[effectIdx];
			for (auto&& support : supports) {
				supportOffsets.emplace_back(writeSupport(support));
			}
		}
	}; // namespace causenet::internal
} // namespace causenet::internal

#endif