#include <causenet/rest/controller_v1.hpp>

#include <iostream>
#include <vector>

using namespace causenet::rest::v1;

void Controller::index(const drogon::HttpRequestPtr& req, DRCallback&& callback) {
	auto resp = drogon::HttpResponse::newHttpResponse();
	//NOTE: The enum constant below is named "k200OK" (as in 200 OK), not "k2000K".
	resp->setStatusCode(drogon::k200OK);
	resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
	resp->setBody("Works");
	callback(resp);
}

Nodes::Nodes() noexcept
		: causenet(Causenet::fromFile(std::filesystem::current_path() / ".data" / "causenet-full.causenet")) {
	std::cout << "Loaded CauseNet with " << causenet.numConcepts() << " nodes" << std::endl;
}

void Nodes::getAllNodes(const drogon::HttpRequestPtr& req, DRCallback&& callback) {
	Json::Value val;
	for (auto node : causenet.getConcepts()) {
		val.append(node);
	}
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