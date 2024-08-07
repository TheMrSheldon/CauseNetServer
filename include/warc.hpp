#ifndef WARC_HPP
#define WARC_HPP

#include <algorithm>
#include <cassert>
#include <istream>
#include <map>

namespace warc::v1 {
	struct WARCRecord {
		std::map<std::string, std::string> entries;
		std::string content;
	};

	inline bool onlyWhitespace(std::string_view str) {
		return std::all_of(str.begin(), str.end(), [](char c) { return std::isspace(c); });
	}

	inline std::string_view trim(std::string_view str) {
		auto begin = std::find_if(str.begin(), str.end(), [](char ch) {
			return !std::isspace<char>(ch, std::locale::classic());
		});
		auto rend = std::find_if(str.rbegin(), str.rend(), [](char ch) {
			return !std::isspace<char>(ch, std::locale::classic());
		});
		return std::string_view(begin, rend.base());
	}

	inline std::pair<std::string, std::string> split(std::string str, char delim) {
		auto dit = str.begin() + str.find(delim);
		std::string key(trim(std::string_view{str.begin(), dit}));
		std::string value(trim(std::string_view{dit + 1, str.end()}));
		return {key, value};
	}

	inline std::istream& operator>>(std::istream& is, WARCRecord& record) {
		std::istream::sentry s(is);
		if (s) {
			record.entries.clear();
			std::string line;
			/** \todo future-me could do better error handling **/
			is >> line;
			assert(line == "WARC/1.0");
			for (std::istream::sentry s2(is); std::getline(is, line) && !onlyWhitespace(line);)
				record.entries.emplace(split(line, ':'));
			size_t length = std::strtoull(record.entries["Content-Length"].c_str(), nullptr, 10);
			record.content.resize(length);
			assert(is.read(record.content.data(), record.content.size()));
		}
		return is;
	}
} // namespace warc::v1

#endif