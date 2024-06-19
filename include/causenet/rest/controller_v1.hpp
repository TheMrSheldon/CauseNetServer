#pragma once

#include <causenet/causenet.hpp>

#include <drogon/HttpController.h>

namespace causenet::rest::v1 {
	class Controller : public drogon::HttpController<Controller> {
		using DRCallback = std::function<void(const drogon::HttpResponsePtr&)>;

	public:
		METHOD_LIST_BEGIN
		ADD_METHOD_TO(Controller::index, "/", drogon::Get);
		METHOD_LIST_END

		void index(const drogon::HttpRequestPtr& req, DRCallback&& callback);
	};

	class Nodes : public drogon::HttpController<Nodes> {
		using DRCallback = std::function<void(const drogon::HttpResponsePtr&)>;

	private:
		causenet::Causenet causenet;

	public:
		Nodes() noexcept;

		METHOD_LIST_BEGIN
		ADD_METHOD_TO(Nodes::getAllNodes, "/v1/nodes", drogon::Get);
		ADD_METHOD_TO(Nodes::getNode, "/v1/nodes/{nodeid}", drogon::Get);
		METHOD_LIST_END

		void getAllNodes(const drogon::HttpRequestPtr& req, DRCallback&& callback);
		void getNode(const drogon::HttpRequestPtr& req, DRCallback&& callback, std::string nodeid);
	};
} // namespace causenet::rest::v1