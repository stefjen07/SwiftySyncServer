#ifndef SWIFTYSYNC_H
#define SWIFTYSYNC_H

#define SERVER

#include <Usage.h>
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#include <uwebsockets/App.h>
#endif
#include <SwiftySyncStorage.h>
#include <Codable.h>
#include <JSON.h>
#include <Authorization.h>
#include <UUID.h>
#include <Request.h>
#include <Functions.h>
#include <vector>
#include <string>
#include <functional>
#include <fstream>
#include <experimental/filesystem>
#include <iostream>
#include <timercpp.h>
#include <algorithm>
#if !(defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__))
#include <uwebsockets/App.h>
#endif

using namespace std;

class ConnectionData {
public:
	string connectionId;
	string userId;
};

bool isDataRequest(Request* request);

struct SecurityRule {
	function<bool(DataRequest*)> dataRule;
	function<bool(FunctionRequest*)> functionRule;

	bool checkAccess(Request* request);
};


#ifdef SSL_SERVER
typedef uWS::WebSocket<1, 1, ConnectionData>* WebSocket;
#else
typedef uWS::WebSocket<0, 1, ConnectionData>* WebSocket;
#endif

struct ServerBehavior {
	function<void(bool)> completion = [](auto result) {};
	function<void(WebSocket)> connectionOpened = [](auto ws) {};
	function<void(WebSocket)> messageReceived = [](auto ws) {};
	function<void(AuthorizationStatus)> authorized = [](auto as) {};
};

struct RunBehavior {
	function<void()> afterStart = []() {};
	function<void()> update = []() {};

	unsigned updateInterval = 1000;
	string key_filename;
	string cert_filename;
	string passphrase;
};

class SwiftyServer {
public:
	string address;
	int port;
	string serverUrl = "";

	vector<Collection> collections;
	vector<Function> functions;
	vector<AuthorizationProvider*> supportedProviders;
	ServerBehavior behavior;

	SecurityRule rule;
	
	Collection* operator [](string name);

	void read();

	void save();

	void authorizeWithStatus(WebSocket ws, AuthorizationStatus status);

	void authorize(WebSocket ws, string body);

	void handleDataRequest(WebSocket ws, DataRequest* request);

	void handleFunctionRequest(WebSocket ws, FunctionRequest* request);

	DataRequest lastDataRequest;
	FunctionRequest lastFunctionRequest;
	bool handlingRequest = false;

	void handleRequest(WebSocket ws, Request* request);

	Request* generateRequest(WebSocket ws, string body);

	void handleMessage(WebSocket ws, string_view message);

	void run(RunBehavior runBehavior);

	SwiftyServer(string address, int port, ServerBehavior behavior) {
		this->address = address;
		this->port = port;
		this->behavior = behavior;
	}
};

#endif
