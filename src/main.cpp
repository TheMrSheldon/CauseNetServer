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

#if 0
#include <causenet/causenet.hpp>

#include <utils/shortest_paths.hpp>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>

using causenet::Causenet;

int main(int argc, char* argv[]) {
	// auto jsonpath = std::filesystem::current_path() / ".data" / "causenet-full.jsonl";
	auto binpath = std::filesystem::current_path() / ".data" / "causenet-full.causenet";
	// Causenet::jsonlToBinary(jsonpath, binpath);
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