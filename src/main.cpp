#include <causenet/causenet.hpp>
#include <warc.hpp>

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

using Causenet = causenet::Causenet;

/*std::string getWikipediaPermalink(std::string curId, std::string timestamp) {
	auto baseUrl = "https://en.wikipedia.org/w";
	auto url = std::format(
			"{}/api.php?action=query&prop=revisions&rvprop=timestamp|ids&rvdir=newer&pageids={}&rvstart={}&rvlimit=1",
			baseUrl, curId, timestamp
	);
}*/
// curId = "35296365"
// timestamp = "2012-04-02T00:10:59Z"
/*
std::string warcId2TrecID(std::filesystem::path cluewebBase) {}

int main() {
	std::set<std::string> clueWebIds;

	auto causenet = Causenet::fromFile(".data/causenet-full-supported.causenet");
	for (size_t srcIdx = 0; srcIdx < causenet.numConcepts(); ++srcIdx) {
		for (auto&& [dstIdx, _] : causenet.getEffects(srcIdx))
			for (auto&& support : causenet.getSupport(srcIdx, dstIdx))
				if (support.sourceTypeId == causenet::SourceType::ClueWeb12Sentence)
					clueWebIds.emplace(support.id);
		std::cout << "\rNum distinct ClueWeb Records: " << clueWebIds.size() << std::flush;
	}
	std::cout << "Num distinct ClueWeb Records: " << clueWebIds.size() << std::endl;
	std::ofstream out("./warc-record-ids.txt");
	for (auto&& id : clueWebIds)
		out << id << std::endl;

	/*warc::v1::WARCRecord record;
	std::cout << "Mapping WARC Record IDs to ClueWeb12 TREC IDs..." << std::endl;
	//std::filesystem::path base = "/mnt/ceph/storage/corpora/corpora-thirdparty/corpus-clueweb12/parts";
	std::filesystem::path base = "/Volumes/storage/corpora/corpora-thirdparty/corpus-clueweb12/parts";
	size_t progress = 0;
	for (auto const& path : std::filesystem::recursive_directory_iterator(base)) {
		std::ifstream file(path.path());
		assert(file.good());
		boost::iostreams::filtering_istream stream{boost::iostreams::gzip_decompressor()};
		stream.push(file);
		stream >> record;
		assert(record.entries["WARC-Type"] == "warcinfo");
		size_t numInArchive = std::strtoull(record.entries["WARC-File-Length"].c_str(), nullptr, 10);
		for (size_t i = 0; i < numInArchive; ++i) {
			stream >> record;
		}
		std::cout << "\r" << progress << std::flush;
		++progress;
	}
	std::cout << std::endl;

	return 0;*/
//}

#include <drogon/HttpAppFramework.h>

#include <causenet/rest/controller_v1.hpp>

int main() {
	// Set HTTP listener address and port
	drogon::app().addListener("0.0.0.0", 8080);
	// Load config file
	drogon::app().loadConfigFile("config.dev.yml");
	// Run HTTP framework,the method will block in the internal event loop
	drogon::app().run();
	return 0;
}

/**
 * 1. Is this concept fictional or non-fictional
 * 2. Is thie concept a person or not
 * ...
 */

/**
 * Experiments for filtering:
 * - Entity Resolution: Use concept name to search in DBPedia or Wikidata
 * - 
 * 
 * None of the above
 */

/**
 * Check recall of "Query Interpretations from Entity-Linked Segmentations"
 * 
 */

/**
 * Stepweise approach for the processing steps
 * 
 */

#if 0
#include <causenet/causenet.hpp>

#include <utils/shortest_paths.hpp>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>

using causenet::Causenet;

int main(int argc, char* argv[]) {
	auto causenet = Causenet::fromFile(binpath);
	auto deathIdx = causenet.getConceptIdx("death");

	auto neighborfn = std::bind(&Causenet::getEffects, std::cref(causenet), std::placeholders::_1);
	/*auto neighborfn = [causenet](size_t idx) -> Generator<std::tuple<size_t, float>> {
		for (auto&& [neighbor, _] : causenet.getEffects(idx)) {
			auto numNeighbors = causenet.numEffects(neighbor);
			co_yield std::make_tuple(neighbor, (float)numNeighbors);
		}
	};*/
	// auto start = causenet.getConceptIdx("investment");
	auto start = causenet.getConceptIdx("mushrooms");
	auto target = causenet.getConceptIdx("death");
	if (start == -1)
		abort();
	for (const auto& node : utils::shortestPath(start, target, neighborfn)) {
		std::cout << causenet.getConceptByIdx(node) << std::endl;
	}

	// Compute the number of connected components
	//auto components = utils::connectedComponents(causenet.numConcepts(), neighborfn);
	//auto count = utils::count(components);
	//std::cout << "Num connected components: " << count.size() << std::endl; // 198
	//std::cout << "Num connected components: " << (unsigned)*std::max_element(components.begin(), components.end())
	//		  << std::endl; // 198

	// Compute all shortest Paths
	/*auto dist = utils::floydWarshall(
			causenet.numConcepts(), std::bind(&Causenet::getEffects, std::cref(causenet), std::placeholders::_1)
	);
	std::cout << "Length of longest path: " << *std::max_element(dist.begin(), dist.end()) << std::endl;*/

	/*auto target = causenet.getConceptIdx("death");
	auto neighborfn = std::bind(&Causenet::getEffects, std::cref(causenet), std::placeholders::_1);
	for (size_t start = 0; start < causenet.numConcepts(); ++start) {
		auto path = utils::shortestPath(start, target, neighborfn);
		if (path.size() > 9) {
			std::cout << causenet.getConceptByIdx(start) << "--(" << path.size() << ")-->"
					  << causenet.getConceptByIdx(target) << std::endl;
			for (const auto& node : path)
				std::cout << causenet.getConceptByIdx(node) << std::endl;
		}
	}*/

	return 0;
}
#endif