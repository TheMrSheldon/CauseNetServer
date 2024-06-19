#ifndef UTILS_SHORTESTPATHS_HPP
#define UTILS_SHORTESTPATHS_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <map>
#include <queue>
#include <ranges>
#include <set>
#include <vector>

#include "generator.hpp"

namespace utils {

	template <typename T, typename Node>
	concept NeighborFn = requires(T fn, Node n) {
		/*requires std::ranges::range<decltype(fn(n))>;*/ // Don't know why but utils::Generator does not fullfill this
		fn(n);
	};

	template <typename Node, typename F>
		requires NeighborFn<F, Node>
	inline std::vector<Node> shortestPath(Node start, Node target, F neighborfn) {
		using Elem = std::tuple<int, Node>;
		std::priority_queue<Elem, std::vector<Elem>, std::greater<Elem>> queue;
		std::map<Node, Node> previous;
		std::set<Node> visited;
		queue.emplace(0, start);
		for (; !(queue.empty() || std::get<1>(queue.top()) == target); queue.pop()) {
			const auto [dist, top] = queue.top();
			for (auto&& [neighbor, weight] : neighborfn(top)) {
				const auto& [_, newNode] = visited.insert(neighbor);
				if (newNode) {
					queue.emplace(dist + weight, neighbor);
					previous[neighbor] = top;
				}
			}
		}
		if (!queue.empty()) {
			std::vector<Node> path = {target};
			do {
				path.emplace_back(previous[path.back()]);
			} while (path.back() != start);
			std::reverse(path.begin(), path.end());
			return path;
		}
		return {};
	}

	template <typename F, typename Counter = size_t>
		requires NeighborFn<F, size_t>
	inline std::vector<Counter> connectedComponents(size_t numNodes, F neighborFn) {
		constexpr Counter unassigned{};
		std::vector<Counter> marks(numNodes, unassigned);
		Counter count = unassigned + 1;
		for (typename decltype(marks)::iterator it;
			 (it = std::find(marks.begin(), marks.end(), unassigned)) != marks.end(); ++count) {
			std::cout << "Component: " << (int)count << std::endl;
			std::queue<size_t> next;
			auto idx = std::distance(marks.begin(), it);
			next.push(idx);
			marks[idx] = count;
			while (!next.empty()) {
				idx = next.front();
				next.pop();
				for (const auto&& [neighbor, weight] : neighborFn(idx)) {
					// Assert: the neighbor cant be in another connected component
					assert(marks[neighbor] == unassigned || marks[neighbor] == count);
					if (marks[neighbor] == unassigned) {
						next.push(neighbor);
						marks[neighbor] = count;
					}
				}
			}
		}
		return marks;
	}

	inline auto count(std::ranges::range auto& range) -> std::map<std::ranges::range_value_t<decltype(range)>, size_t> {
		using K = std::ranges::range_value_t<decltype(range)>;
		std::map<K, size_t> ret;
		for (const auto& element : range)
			ret[element]++;
		return ret;
	}

	/*template <typename F>
		requires NeighborFn<F, size_t>
	inline std::vector<float> floydWarshall(size_t numNodes, F neighborfn) {
		std::vector<float> dist(numNodes * numNodes, 0.0f);
		for (size_t i = 0; i < numNodes; ++i) {
			for (auto&& [j, weight] : neighborfn(i))
				dist[j + i * numNodes] = (float)weight;
			dist[i + i * numNodes] = 0;
		}
		for (size_t k = 0; k < numNodes; ++k) {
			for (size_t i = 0; i < numNodes; ++i) {
				for (size_t j = 0; j < numNodes; ++j) {
					dist[j + i * numNodes] =
							std::min(dist[j + i * numNodes], dist[k + i * numNodes] + dist[j + k * numNodes]);
				}
			}
		}
	}*/
} // namespace utils

#endif