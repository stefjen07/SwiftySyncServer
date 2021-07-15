#ifndef SWIFTYSYNC_H
#define SWIFTYSYNC_H

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING

#include "uwebsockets/App.h"
#include "Codable.h"
#include "JSON.h"
#include "Authorization.h"
#include "Helper.h"
#include <vector>
#include <string>
#include <functional>
#include <fstream>
#include <experimental/filesystem>
#include <iostream>
#include <timercpp.h>
#include <algorithm>

#define AUTH_PREFIX "A"
#define REQUEST_PREFIX "R"
#define DOCUMENT_GET_PREFIX "DG"
#define DOCUMENT_SET_PREFIX "DS"
#define FIELD_GET_PREFIX "FG"
#define FIELD_SET_PREFIX "FS"
#define DATA_REQUEST_PREFIX "DR"
#define DATA_SET_SUCCESSFUL "DSS"
#define FIELD_SET_SUCCESSFUL "FSS"
#define DATA_REQUEST_FAILURE "DRF"
#define DOCUMENT_EXTENSION "document"

namespace fs = std::experimental::filesystem;
using namespace std;

enum class FieldType {
	array=0,
	number,
	string
};

class Field: public Codable {
public:
	FieldType type;
	string name;
	double numValue;
	string strValue;
	vector<Field> children;

	Field* operator [](string key) {
		for (int i = 0; i < children.size(); i++) {
			if (children[i].name == key) {
				return &children[i];
			}
		}
		return NULL;
	}

	void addChild(Field child) {
		if (type == FieldType::array) {
			children.push_back(child);
		}
	}

	void encode(CoderContainer* container) {
		if (container->type == CoderType::json) {
			JSONEncodeContainer* jsonContainer = dynamic_cast<JSONEncodeContainer*>(container);
			jsonContainer->encode(int(type), name + "type");
			if (type == FieldType::array) {
				jsonContainer->encode(children, name);
			}
			else if (type == FieldType::string) {
				jsonContainer->encode(strValue, name);
			}
			else if (type == FieldType::number) {
				jsonContainer->encode(numValue, name);
			}
		}
	}

	void decode(CoderContainer* container) {
		if (container->type == CoderType::json) {
			JSONDecodeContainer* jsonContainer = dynamic_cast<JSONDecodeContainer*>(container);
			type = FieldType(jsonContainer->decode(int(), name + "type"));
			if (type == FieldType::array) {
				children = jsonContainer->decode(vector<Field>(), name);
			}
			else if(type == FieldType::string) {
				strValue = jsonContainer->decode(string(), name);
			}
			else if (type == FieldType::number) {
				numValue = jsonContainer->decode(double(), name);
			}
		}
	}

	Field() {}

	Field(FieldType type, string name) {
		this->type = type;
		this->name = name;
	}
};

class Collection;

class Document {
public:
	string name;
	vector<Field> fields;
	Collection* collection;

	string documentUrl();

	Field* operator [](string name) {
		for (int i = 0; i < fields.size(); i++) {
			return &fields[i];
		}
		return NULL;
	}

	void read();
	void save();

	Document(Collection* collection, string name) {
		this->name = name;
		this->collection = collection;
	}

	Document() {

	}
};

class SwiftyServer;

class Collection {
public:
	string name;
	vector<Document> documents;
	SwiftyServer* server;

	string collectionUrl();

	function<void()> onDocumentCreating = []() {};

	Document* operator [](string name) {
		for (int i = 0; i < documents.size(); i++) {
			return &documents[i];
		}
		return NULL;
	}

	bool isDocumentNameTaken(string name) {
		for (int i = 0; i < documents.size(); i++) {
			if (name == documents[i].name) {
				return true;
			}
		}
		return false;
	}

	void read();
	void save();
	void createDocument(string name) {
		if(isDocumentNameTaken(name)) {
			return;
		}
		auto document = Document(this, name);
		documents.push_back(document);
		save();
		if (onDocumentCreating != NULL) {
			onDocumentCreating();
		}
	}

	Collection(SwiftyServer* server, string name) {
		this->name = name;
		this->server = server;
	}
};

enum class RequestType {
	documentSet,
	documentGet,
	fieldSet,
	fieldGet,
	undefined
};

struct ConnectionData {
	string connectionId;
	string userId;
};

class Request {
public:
	RequestType type;
	ConnectionData connection;
	virtual void nothing() {};
};

class DataRequest : public Request, public Codable {
public:
	string collectionName;
	string documentName;
	string body;

	void nothing() {}

	void encode(CoderContainer* container) {
		if (container->type == CoderType::json) {
			JSONEncodeContainer* jsonContainer = dynamic_cast<JSONEncodeContainer*>(container);
			jsonContainer->encode(collectionName, "collection");
			jsonContainer->encode(documentName, "document");
			jsonContainer->encode(body, "body");
		}
	}

	void decode(CoderContainer* container) {
		if (container->type == CoderType::json) {
			JSONDecodeContainer* jsonContainer = dynamic_cast<JSONDecodeContainer*>(container);
			collectionName = jsonContainer->decode(string(), "collection");
			documentName = jsonContainer->decode(string(), "document");
			body = jsonContainer->decode(string(), "body");
		}
	}

	DataRequest() {}
};

class FieldRequest : public Codable {
public:
	string value;
	vector<string> path;

	void encode(CoderContainer* container) {
		if (container->type == CoderType::json) {
			JSONEncodeContainer* jsonContainer = dynamic_cast<JSONEncodeContainer*>(container);
			jsonContainer->encode(value, "value");
			jsonContainer->encode(path, "path");
		}
	}

	void decode(CoderContainer* container) {
		if (container->type == CoderType::json) {
			JSONDecodeContainer* jsonContainer = dynamic_cast<JSONDecodeContainer*>(container);
			value = jsonContainer->decode(string(), "value");
			path = jsonContainer->decode(vector<string>(), "path");
		}
	}
};

const vector<RequestType> DATA_REQUEST_TYPES = { RequestType::documentSet, RequestType::documentGet, RequestType::fieldSet, RequestType::fieldGet };
#define DATA_REQUEST_TYPES_COUNT 4

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
			dataResult.connection = *data;
			dataResult.type = requestType;
			lastDataRequest = dataResult;
			return &lastDataRequest;
		}

		return &tempRequest;
	}

	void handleMessage(WebSocket ws, string_view message) {
		string wrappedMessage = string(message);
		if (wrappedMessage.find(AUTH_PREFIX) == 0) {
			authorize(ws, wrappedMessage.substr(strlen(AUTH_PREFIX), wrappedMessage.length() - strlen(AUTH_PREFIX)));
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

void Collection::save() {
	if (name.empty()) {
		return;
	}
	string url = collectionUrl();
	url.erase(url.end() - 1);
	fs::create_directory(url);
	for (auto document : documents) {
		document.save();
	}
}

string Document::documentUrl() {
	return collection->collectionUrl() + "/" + name + "." + DOCUMENT_EXTENSION;
}

void Document::read() {
	ifstream documentStream(documentUrl());
	string content;
	getline(documentStream, content);
	auto decoder = JSONDecoder();
	auto container = decoder.container(content);
	auto decoded = container.decode(vector<Field>());
	this->fields = decoded;
}

void Document::save() {
	ofstream documentStream(documentUrl());
	auto encoder = JSONEncoder();
	auto container = encoder.container();
	container.encode(fields);
	documentStream << container.content;
}

void Collection::read() {
	string url = collectionUrl();
	for (auto& entry : fs::directory_iterator(url)) {
		string filename = entry.path().stem().string();
		createDocument(filename);
	}
	for (int i = 0; i < documents.size(); i++) {
		documents[i].read();
	}
}

#endif