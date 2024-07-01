#pragma once

#include <causenet/causenet.hpp>

#include <drogon/HttpController.h>

#include <memory>

struct CausenetWrapper {
private:
	causenet::Causenet causenet;

public:
	CausenetWrapper(std::filesystem::path path) : causenet(causenet::Causenet::fromFile(path)) {}

	causenet::Causenet& get() { return causenet; }
};

namespace causenet::rest::v1 {
	class Controller : public drogon::HttpController<Controller> {
		using DRCallback = std::function<void(const drogon::HttpResponsePtr&)>;

	public:
		static std::unique_ptr<CausenetWrapper> causenet;

		Controller() noexcept;

		METHOD_LIST_BEGIN
		ADD_METHOD_TO(Controller::index, "/", drogon::Get);
		METHOD_LIST_END

		void index(const drogon::HttpRequestPtr& req, DRCallback&& callback);
	};

	class Nodes : public drogon::HttpController<Nodes> {
		using DRCallback = std::function<void(const drogon::HttpResponsePtr&)>;

	private:
		causenet::Causenet& causenet;

	public:
		Nodes() noexcept;

		METHOD_LIST_BEGIN
		ADD_METHOD_TO(Nodes::getAllNodes, "/v1/nodes", drogon::Get);
		ADD_METHOD_TO(Nodes::getNode, "/v1/nodes/{nodeid}", drogon::Get);
		ADD_METHOD_TO(Nodes::getEffects, "/v1/nodes/{nodeid}/effects", drogon::Get);
		ADD_METHOD_TO(Nodes::getEffect, "/v1/nodes/{nodeid}/effects/{targetid}", drogon::Get);
		ADD_METHOD_TO(Nodes::getPath, "/v1/nodes/{nodeid}/path-to/{targetid}", drogon::Get);
		METHOD_LIST_END

		void getAllNodes(const drogon::HttpRequestPtr& req, DRCallback&& callback);
		void getNode(const drogon::HttpRequestPtr& req, DRCallback&& callback, std::string nodeid);
		void getEffects(const drogon::HttpRequestPtr& req, DRCallback&& callback, std::string nodeid);
		void
		getEffect(const drogon::HttpRequestPtr& req, DRCallback&& callback, std::string nodeid, std::string targetid);
		void
		getPath(const drogon::HttpRequestPtr& req, DRCallback&& callback, std::string nodeid, std::string targetid);
	};
} // namespace causenet::rest::v1