#ifndef CAUSENET_CAUSENETFILE_HPP
#define CAUSENET_CAUSENETFILE_HPP

#include <utils/generator.hpp>

#include <cinttypes>

using offset_t = std::uint64_t;

struct __attribute__((packed)) Header {
	std::size_t numNodes;
	std::size_t conceptOffset;
	std::size_t infoOffset;
	std::size_t supportOffset;

	inline const char* nodeBase() const noexcept { return reinterpret_cast<const char*>(this) + conceptOffset; }
	inline const char* nodeInfoBase() const noexcept { return reinterpret_cast<const char*>(this) + infoOffset; }
	inline const char* supportBase() const noexcept { return reinterpret_cast<const char*>(this) + supportOffset; }
};
static_assert(sizeof(Header) == 32);

struct __attribute__((packed)) EdgeEntry {
	uint32_t targetIdx;
	uint32_t numSupport;
	offset_t supportOffset;

	inline Generator<causenet::Support> support(const Header& file) const {
		const offset_t* base = reinterpret_cast<const offset_t*>(file.nodeInfoBase() + supportOffset);
		causenet::Support support;
		for (size_t i = 0; i < numSupport; ++i) {
			const char* data = file.supportBase() + base[i];
			support.sourceTypeId = *reinterpret_cast<const causenet::SourceType*>(data);
			data += sizeof(support.sourceTypeId);
			support.id = std::string(data);
			data += support.id.length() + 1;
			support.content = std::string(data);
			data += support.content.length() + 1;
			co_yield std::move(support);
		}
	}
};
static_assert(sizeof(EdgeEntry) == 16);

static const EdgeEntry nulledge = {.targetIdx = (uint32_t)-1, .numSupport = 0, .supportOffset = 0};

struct __attribute__((packed)) NodeEntry {
	offset_t nameOffset;
	offset_t effectOffset;

	inline const char* name(const Header& file) const { return file.nodeInfoBase() + nameOffset; }
	inline const EdgeEntry* effects(const Header& file) const {
		return reinterpret_cast<const EdgeEntry*>(file.nodeInfoBase() + effectOffset);
	}
};
static_assert(sizeof(NodeEntry) == 16);

#endif