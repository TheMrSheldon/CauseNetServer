#ifndef CAUSENET_SUPPORT_HPP
#define CAUSENET_SUPPORT_HPP

#include <cinttypes>
#include <string>

namespace causenet {
	enum class SourceType : std::uint8_t { WikipediaInfobox, WikipediaList, WikipediaSentence, ClueWeb12Sentence };

	struct Support {
		SourceType sourceTypeId;
		std::string id;
		std::string content;

		bool operator==(const Support& other) const noexcept {
			return sourceTypeId == other.sourceTypeId && id == other.id && content == other.content;
		}
	};
} // namespace causenet

#endif