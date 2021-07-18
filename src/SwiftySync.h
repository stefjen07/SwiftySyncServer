#ifndef SWIFTYSYNC_H
#define SWIFTYSYNC_H

#define SERVER

#include <uwebsockets/App.h>
#include <SwiftySyncStorage.h>
#include <Codable.h>
#include <JSON.h>
#include <Authorization.h>
#include <Helper.h>
#include <Request.h>
#include <vector>
#include <string>
#include <functional>
#include <fstream>
#include <experimental/filesystem>
#include <iostream>
#include <timercpp.h>
#include <algorithm>

using namespace std;

class ConnectionData {
public:
	string connectionId;
	string userId;
};

bool isDataRequest(Request* request) {
	for (int i = 0; i < DATA_REQUEST_TYPES_COUNT; i++) {
		if (DATA_REQUEST_TYPES[i] == request->type) {
			return true;
		}
	}
	return false;
}

struct SecurityRule {
	function<bool(DataRequest*)> dataRule;

	bool checkAccess(Request* request) {
		if (isDataRequest(request)) {
			DataRequest* dataRequest = dynamic_cast<DataRequest*>(request);
			return dataRule(dataRequest);
		}
		return false;
	}
};

typedef uWS::WebSocket<0, 1, ConnectionData>* WebSocket;

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
};

class SwiftyServer {
public:
	string address;
	int port;
	string serverUrl = "";

	vector<Collection> collections;
	vector<AuthorizationProvider*> supportedProviders;
	ServerBehavior behavior;

	SecurityRule rule;
	
	Collection* operator [](string name) {
		for (int i = 0; i < collections.size(); i++) {
			if (name == collections[i].name) {
				return &collections[i];
			}
		}
		return NULL;
	}

	void read() {
		for (int i = 0; i < collections.size(); i++) {
			collections[i].read();
		}
	}

	void save() {
		for (auto collection : collections) {
			collection.save();
		}
	}

	void authorizeWithStatus(WebSocket ws, AuthorizationStatus status) {
		ConnectionData* data = (ConnectionData*)ws->getUserData();
		string respond = AUTH_PREFIX;
		if (status == AuthorizationStatus::authorized) {
			respond += AUTHORIZED_LOCALIZE;
		}
		else if (status == AuthorizationStatus::corruptedCredentials) {
			respond += CORR_CRED_LOCALIZE;
		}
		else if (status == AuthorizationStatus::error) {
			respond += AUTH_ERR_LOCALIZE;
		}
		ws->send(respond);
		behavior.authorized(status);
	}

	void authorize(WebSocket ws, string body) {
		ConnectionData* data = (ConnectionData*)ws->getUserData();
		for (auto provider : supportedProviders) {
			if (provider->isValid(body)) {
				auto response = provider->authorize(body);
				if (response.status == AuthorizationStatus::authorized) {
					data->userId = response.userId;
				}
				authorizeWithStatus(ws, response.status);
				return;
			}
		}
		authorizeWithStatus(ws, AuthorizationStatus::corruptedCredentials);
	}

	void handleDataRequest(WebSocket ws, DataRequest* request) {
		string respond;
		respond += REQUEST_PREFIX;
		respond += DATA_REQUEST_PREFIX;
		auto collection = operator[](request->collectionName);
		if (collection == nullptr) {
			ws->send(respond + DATA_REQUEST_FAILURE);
			return;
		}
		collection->createDocument(request->documentName);
		auto doc = collection->operator[](request->documentName);
		if (request->type == RequestType::documentGet) {
			JSONEncoder encoder;
			auto container = encoder.container();
			container.encode(doc->fields);
			respond += container.content;
		}
		if (request->type == RequestType::documentSet) {
			JSONDecoder decoder;
			auto container = decoder.container(request->body);
			auto fields = container.decode(vector<Field>());
			doc->fields = fields;
			respond += DATA_SET_SUCCESSFUL;
		}
		if (request->type == RequestType::fieldGet) {
			JSONEncoder encoder;
			JSONDecoder decoder;
			auto decodeContainer = decoder.container(request->body);
			auto fieldRequest = decodeContainer.decode(FieldRequest());
			if (!fieldRequest.path.empty()) {
				Field* lastField = nullptr;
				for (int i = 0; i < doc->fields.size(); i++) {
					if (doc->fields[i].name == fieldRequest.path[0]) {
						lastField = &doc->fields[i];
					}
				}
				for (int i = 1; i < fieldRequest.path.size(); i++) {
					if (lastField != nullptr) {
						lastField = lastField->operator[](fieldRequest.path[i]);
					}
				}
				if (lastField != nullptr) {
					auto encodeContainer = encoder.container();
					encodeContainer.encode(*lastField);
					respond += encodeContainer.content;
				}
			}
		}
		if (request->type == RequestType::fieldSet) {
			JSONDecoder decoder;
			auto container = decoder.container(request->body);
			auto fieldRequest = container.decode(FieldRequest());
			JSONDecoder valueDecoder;
			auto valueContainer = valueDecoder.container(fieldRequest.value);
			auto fieldValue = container.decode(Field());
			if (!fieldRequest.path.empty()) {
				Field* lastField = nullptr;
				for (int i = 0; i < doc->fields.size(); i++) {
					if (doc->fields[i].name == fieldRequest.path[0]) {
						lastField = &doc->fields[i];
					}
				}
				for (int i = 1; i < fieldRequest.path.size(); i++) {
					if (lastField != nullptr) {
						lastField = lastField->operator[](fieldRequest.path[i]);
					}
				}
				if (lastField != nullptr) {
					*lastField = fieldValue;
				}
			}
			respond += FIELD_SET_SUCCESSFUL;
		}
		ws->send(respond);
	}

	DataRequest lastDataRequest;
	bool handlingRequest = false;

	void handleRequest(WebSocket ws, Request* request) {
		if (rule.checkAccess(request)) {
			if (isDataRequest(request)) {
				auto dataRequest = (DataRequest*) dynamic_cast<DataRequest*>(request);
				handleDataRequest(ws, dataRequest);
			}
		}
		else {
			cout << "Access denied\n";
		}
		handlingRequest = false;
	}

	Request* generateRequest(WebSocket ws, string body) {
		while (handlingRequest)
			continue;
		handlingRequest = true;

		RequestType requestType = RequestType::undefined;

		ConnectionData* data = (ConnectionData*)ws->getUserData();
		JSONDecoder decoder;
		unsigned prefixSize;

		if (body.find(DOCUMENT_GET_PREFIX) == 0) {
			requestType = RequestType::documentGet;
			prefixSize = strlen(DOCUMENT_GET_PREFIX);
		}

		if (body.find(DOCUMENT_SET_PREFIX) == 0) {
			requestType = RequestType::documentSet;
			prefixSize = strlen(DOCUMENT_SET_PREFIX);
		}

		if (body.find(FIELD_GET_PREFIX) == 0) {
			requestType = RequestType::fieldGet;
			prefixSize = strlen(FIELD_GET_PREFIX);
		}

		if (body.find(FIELD_SET_PREFIX) == 0) {
			requestType = RequestType::fieldSet;
			prefixSize = strlen(FIELD_SET_PREFIX);
		}

		Request tempRequest;
		tempRequest.type = requestType;

		if (isDataRequest(&tempRequest)) {
			string encoded = body.substr(prefixSize, body.length() - prefixSize);
			auto container = decoder.container(encoded);
			auto dataResult = container.decode(DataRequest());
			dataResult.connection = data;
			dataResult.type = requestType;
			lastDataRequest = dataResult;
			return &lastDataRequest;
		}

		return &tempRequest;
	}

	void handleMessage(WebSocket ws, string_view message) {
		ConnectionData* data = (ConnectionData*)ws->getUserData();
		string wrappedMessage = string(message);
		if (wrappedMessage.find(AUTH_PREFIX) == 0) {
			authorize(ws, wrappedMessage.substr(strlen(AUTH_PREFIX), wrappedMessage.length() - strlen(AUTH_PREFIX)));
		}
		else if (data->userId == "") {
			ws->send(string(AUTH_PREFIX) + string(AUTH_ERR_LOCALIZE));
			return;
		}
		if (wrappedMessage.find(REQUEST_PREFIX) == 0) {
			auto request = generateRequest(ws, wrappedMessage.substr(strlen(REQUEST_PREFIX), wrappedMessage.length() - strlen(REQUEST_PREFIX)));
			handleRequest(ws, request);
		}
	}

	void run(RunBehavior runBehavior) {
		bool started = true;
		read();
		save();
		Timer t = Timer();
		t.setInterval(runBehavior.update, runBehavior.updateInterval);
		uWS::App().ws<ConnectionData>("/*", {
			.open = [this](auto* ws) {
				ConnectionData* data = (ConnectionData*)ws->getUserData();
				data->connectionId = create_uuid();
				behavior.connectionOpened(ws);
			},
			.message = [this](auto* ws, string_view message, uWS::OpCode opCode) {
				behavior.messageReceived(ws);
				handleMessage(ws, message);
			}
		}).listen(port, [this, runBehavior](auto* token) {
			behavior.completion(token);
			runBehavior.afterStart();
		}).run();
	}

	SwiftyServer(string address, int port, ServerBehavior behavior) {
		this->address = address;
		this->port = port;
		this->behavior = behavior;
	}
};

string Collection::collectionUrl() {
	return server->serverUrl + name + "/";
}

#endif