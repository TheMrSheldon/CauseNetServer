#ifndef CAUSENET_CAUSENET_HPP
#define CAUSENET_CAUSENET_HPP

#include <filesystem>
#include <string>
#include <tuple>
#include <vector>

#include "../utils/generator.hpp"

namespace causenet {
	enum class SourceType : std::uint8_t { WikipediaInfobox, WikipediaList, WikipediaSentence, ClueWeb12Sentence };

	struct Support {
		SourceType sourceTypeId;
		std::string id;
		std::string content;
	};

	struct CausenetFile;
	class Causenet final {
	private:
		int fd;
		const CausenetFile& file;

		Causenet(const std::filesystem::path& path);

	public:
		size_t getConceptIdx(std::string name) const noexcept;
		const std::string getConceptByIdx(size_t idx) const noexcept;
		size_t numConcepts() const noexcept;
		Generator<std::string> getConcepts() const noexcept;
		Generator<std::tuple<size_t, unsigned>> getEffects(size_t conceptIdx) const noexcept;
		size_t numEffects(size_t conceptIdx) const noexcept;
		std::vector<Support> getSupport(size_t causeIdx, size_t effectIdx) const noexcept;

		static Causenet fromFile(const std::filesystem::path& path);
		static void jsonlToBinary(const std::filesystem::path& inJsonl, std::filesystem::path& outBinary);
	};
} // namespace causenet

#endif