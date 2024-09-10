#include <causenet/rest/controller_v1.hpp>

#include <utils/shortest_paths.hpp>
#include <warc.hpp>

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>

using causenet::Causenet;
using namespace causenet::rest::v1;

std::unique_ptr<CausenetWrapper> Controller::causenet;

Controller::Controller() noexcept {
	Controller::causenet = std::make_unique<CausenetWrapper>(
			std::filesystem::current_path() / ".data" / "causenet-full-supported-reworked.causenet"
	);
	LOG_INFO << "Loaded CauseNet with " << causenet->get().numConcepts() << " nodes";
}

void Controller::index(const drogon::HttpRequestPtr& req, DRCallback&& callback) {
	auto resp = drogon::HttpResponse::newHttpResponse();
	resp->setStatusCode(drogon::k200OK);
	resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
	resp->setBody("Works");
	callback(resp);
}

Nodes::Nodes() noexcept : causenet(Controller::causenet->get()) {}

void Nodes::getAllNodes(const drogon::HttpRequestPtr& req, DRCallback&& callback) {
	Json::Value val;
	for (auto node : causenet.getConcepts())
		val.append(node);
	auto resp = drogon::HttpResponse::newHttpJsonResponse(val);
	resp->setStatusCode(drogon::k200OK);
	callback(resp);
}

void Nodes::getNode(const drogon::HttpRequestPtr& req, DRCallback&& callback, std::string nodeid) {
	auto idx = causenet.getConceptIdx(nodeid);
	if (idx == -1) {
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::k404NotFound);
		callback(resp);
	} else {
		Json::Value val;
		val["name"] = nodeid;
		val["effects"] = Json::Value{};
		for (auto&& [effect, cardinality] : causenet.getEffects(idx)) {
			val["effects"].append(causenet.getConceptByIdx(effect));
		}
		auto resp = drogon::HttpResponse::newHttpJsonResponse(val);
		resp->setStatusCode(drogon::k200OK);
		resp->addHeader("Access-Control-Allow-Origin", "*");
		callback(resp);
	}
}

void Nodes::getEffects(const drogon::HttpRequestPtr& req, DRCallback&& callback, std::string nodeid) {
	auto idx = causenet.getConceptIdx(nodeid);
	if (idx == -1) {
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::k404NotFound);
		callback(resp);
	} else {
		Json::Value val;
		for (auto&& [tgt, support] : causenet.getEffects(idx))
			val.append(causenet.getConceptByIdx(tgt));
		auto resp = drogon::HttpResponse::newHttpJsonResponse(val);
		resp->setStatusCode(drogon::k200OK);
		resp->addHeader("Access-Control-Allow-Origin", "*");
		callback(resp);
	}
}
void Nodes::getEffect(
		const drogon::HttpRequestPtr& req, DRCallback&& callback, std::string nodeid, std::string targetid
) {
	auto srcidx = causenet.getConceptIdx(nodeid);
	auto dstidx = causenet.getConceptIdx(targetid);
	if (srcidx == -1 || dstidx == -1) {
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::k404NotFound);
		callback(resp);
	} else {
		Json::Value val;
		for (auto&& support : causenet.getSupport(srcidx, dstidx)) {
			Json::Value dict;
			dict["sourceTypeId"] = static_cast<std::uint8_t>(support.sourceTypeId);
			dict["id"] = support.id;
			dict["content"] = support.content;
			val.append(dict);
		}
		auto resp = drogon::HttpResponse::newHttpJsonResponse(val);
		resp->setStatusCode(drogon::k200OK);
		resp->addHeader("Access-Control-Allow-Origin", "*");
		callback(resp);
	}
}

void Nodes::getPath(
		const drogon::HttpRequestPtr& req, DRCallback&& callback, std::string nodeid, std::string targetid
) {
	auto start = causenet.getConceptIdx(nodeid);
	auto target = causenet.getConceptIdx(targetid);
	if (start == -1 || target == -1) {
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::k404NotFound);
		callback(resp);
	} else {
		auto neighborfn = std::bind(&Causenet::getEffects, std::cref(causenet), std::placeholders::_1);
		Json::Value val;
		val["path"] = Json::Value{};
		for (const auto& node : utils::shortestPath(start, target, neighborfn))
			val["path"].append(causenet.getConceptByIdx(node));
		auto resp = drogon::HttpResponse::newHttpJsonResponse(val);
		resp->setStatusCode(drogon::k200OK);
		resp->addHeader("Access-Control-Allow-Origin", "*");
		callback(resp);
	}
}

static bool tryGetPath(const std::string& id, const std::filesystem::path& base, std::filesystem::path& path) {
	static std::regex idregex("^clueweb12-(\\d{4}\\w{2})-(\\d{2})-(\\d{5})$");
	std::smatch match;
	if (!std::regex_search(id, match, idregex))
		return false;
	path = base / ("ClueWeb12_" + match[1].str().substr(0, 2)) / match[1].str() /
		   (match[1].str() + "-" + match[2].str() + ".warc.gz");
	return true;
}

static bool tryGetRecordByID(const std::string& id, warc::v1::WARCRecord& record) {
	std::filesystem::path path;
	if (!tryGetPath(id, "/mnt/clueweb12/parts", path))
		return false;
	std::ifstream file(path.c_str());
	if (!file.good())
		return false;
	boost::iostreams::filtering_istream stream{boost::iostreams::gzip_decompressor()};
	stream.push(file);
	while (!stream.eof()) {
		stream >> record;
		if (record.entries["WARC-TREC-ID"] == id)
			return true;
	}
	return false;
}

static std::string loadClueWeb12Entry(const std::string& id) {
	warc::v1::WARCRecord record;
	assert(tryGetRecordByID(id, record));
	auto start = record.content.find("\n\r\n");
	return record.content.substr(start);
}

static std::string redactURLs(std::string text) {
	static std::regex urlregex("(?!\\W)\\w+:\\/\\/[A-z0-9-]+(\\.[A-z0-9-]+){1,}(\\/[A-z0-9-._~:/?#[\\]@!$&'()*+,;=%]*)?"
	);
	return std::regex_replace(text, urlregex, "about:blank");
}

void ClueWeb12::getEntryContent(const drogon::HttpRequestPtr& req, DRCallback&& callback, std::string pageid) {
	auto resp = drogon::HttpResponse::newHttpResponse();
	resp->setBody(redactURLs(loadClueWeb12Entry(pageid)));
	resp->addHeader("Access-Control-Allow-Origin", "*");
	callback(resp);
}

void ClueWeb12::getEntryInfo(const drogon::HttpRequestPtr& req, DRCallback&& callback, std::string pageid) {
	warc::v1::WARCRecord record;
	if (!tryGetRecordByID(pageid, record)) {
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::k404NotFound);
		callback(resp);
		return;
	}
	Json::Value val;
	for (auto&& [key, value] : record.entries)
		val[key] = value;
	auto resp = drogon::HttpResponse::newHttpJsonResponse(val);
	resp->setStatusCode(drogon::k200OK);
	resp->addHeader("Access-Control-Allow-Origin", "*");
	callback(resp);
}