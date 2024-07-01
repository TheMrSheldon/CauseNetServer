#include <causenet/rest/controller_v1.hpp>

#include <utils/shortest_paths.hpp>

#include <iostream>
#include <vector>

using causenet::Causenet;
using namespace causenet::rest::v1;

std::unique_ptr<CausenetWrapper> Controller::causenet;

Controller::Controller() noexcept {
	Controller::causenet = std::make_unique<CausenetWrapper>(
			std::filesystem::current_path() / ".data" / "causenet-full-supported.causenet"
	);
	std::cout << "Loaded CauseNet with " << causenet->get().numConcepts() << " nodes" << std::endl;
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