#ifndef SWIFTYSYNC_H
#define SWIFTYSYNC_H

#define SERVER

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#include <uwebsockets/App.h>
#endif
#include <SwiftySyncStorage.hpp>
#include <Codable.hpp>
#include <JSON.hpp>
#include <Authorization.hpp>
#include <UUID.hpp>
#include <Request.hpp>
#include <Functions.hpp>
#include <vector>
#include <string>
#include <functional>
#include <fstream>
#include <map>
#ifdef __cpp_lib__filesystem
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif
#include <iostream>

#include <algorithm>
#if !(defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__))
#include <uwebsockets/App.h>
#endif

class ConnectionData {
public:
	std::string connectionId;
	std::string userId;
};

bool isDataRequest(Request* request);

struct SecurityRule {
	std::function<bool(DataRequest*)> dataRule;
	std::function<bool(FunctionRequest*)> functionRule;

	bool checkAccess(Request* request);
};


#ifdef SSL_SERVER
typedef uWS::WebSocket<1, 1, ConnectionData>* WebSocket;
#else
typedef uWS::WebSocket<0, 1, ConnectionData>* WebSocket;
#endif

struct ServerBehavior {
	std::function<void(bool)> completion = [](auto result) {};
	std::function<void(WebSocket)> connectionOpened = [](auto ws) {};
    std::function<void(WebSocket)> connectionClosed = [](auto ws) {};
	std::function<void(WebSocket)> messageReceived = [](auto ws) {};
	std::function<void(AuthorizationStatus)> authorized = [](auto as) {};
};

struct RunBehavior {
	std::function<void()> afterStart = []() {};
	std::function<void()> update = []() {};

	unsigned updateInterval = 1000;
	std::string key_filename;
	std::string cert_filename;
	std::string passphrase;
};

class SwiftyServer {
public:
	std::string address;
	int port;
	std::string serverUrl = "";

	std::map<std::string, WebSocket> sockets;

	std::vector<Collection> collections;
	std::vector<Function> functions;
	std::vector<AuthorizationProvider*> supportedProviders;
	ServerBehavior behavior;

	SecurityRule rule;
	
	Collection* operator [](std::string name);

	void read();

	void save();

	void authorizeWithStatus(WebSocket ws, AuthorizationStatus status);

	void authorize(WebSocket ws, std::string body);

	void handleDataRequest(WebSocket ws, DataRequest* request);

	void handleFunctionRequest(WebSocket ws, FunctionRequest* request);

	DataRequest lastDataRequest;
	FunctionRequest lastFunctionRequest;
	bool handlingRequest = false;

	void handleRequest(WebSocket ws, Request* request);

	Request* generateRequest(WebSocket ws, std::string body);

	void handleMessage(WebSocket ws, std::string_view message);

	void sendData(std::string userId, DataUnit data);

	void run(RunBehavior runBehavior);

	SwiftyServer(std::string address, int port, ServerBehavior behavior) {
		this->address = address;
		this->port = port;
		this->behavior = behavior;
	}
};

#endif
