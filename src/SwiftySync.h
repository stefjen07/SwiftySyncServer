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

namespace fs = std::experimental::filesystem;
using namespace std;

enum class FieldType {
	array,
	number,
	string
};

class Field: public Codable {
public:
	FieldType type;
	string name;
	double numValue;
	string strValue;

	void addChild(Field child) {
		if (type == FieldType::array) {
			children.push_back(child);
		}
	}

	void encode(CoderContainer* container) {
		if (container->type == CoderType::json) {
			JSONEncodeContainer* jsonContainer = dynamic_cast<JSONEncodeContainer*>(container);
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

	Field() {

	}

	Field(FieldType type, string name) {
		this->type = type;
		this->name = name;
	}
private:
	vector<Field> children;
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

	Document* operator [](string name) {
		for (int i = 0; i < documents.size(); i++) {
			return &documents[i];
		}
		return NULL;
	}

	void read();
	void save();
	void createDocument(string name) {
		auto document = Document(this, name);
		documents.push_back(document);
		save();
	}

	Collection(SwiftyServer* server, string name) {
		this->name = name;
		this->server = server;
	}
};

class Request {

};

class SecurityRule {
	bool checkAccess(Request request) {

	}
};

struct ConnectionData {
	string connectionId;
	string userId;
};

typedef uWS::WebSocket<0, 1, ConnectionData>* WebSocket;

struct ServerBehavior {
	function<void(bool)> completion;
	function<void(WebSocket)> connectionOpened;
	function<void(WebSocket)> messageReceived;
	function<void(AuthorizationStatus)> authorized;
};

class SwiftyServer {
public:
	string address;
	int port;
	string serverUrl = "";

	vector<Collection> collections;
	vector<AuthorizationProvider*> supportedProviders;
	ServerBehavior behavior;

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
		string respond = "A";
		if (status == AuthorizationStatus::authorized) {
			respond += 'S';
		}
		else if (status == AuthorizationStatus::corruptedCredentials) {
			respond += 'C';
		}
		else if (status == AuthorizationStatus::error) {
			respond += 'E';
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

	void handleMessage(WebSocket ws, string_view message) {
		char type = message[0];
		string body;
		for (int i = 1; i < message.length(); i++) {
			body += message[i];
		}
		switch (type) {
		case 'A':
			authorize(ws, body);
		}
	}

	void run() {
		bool started = true;
		read();
		save();
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
		}).listen(port, [this](auto* token) {
			behavior.completion(token);
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
	fs::create_directory(server->serverUrl + name);
	for (auto document : documents) {
		document.save();
	}
}

string Document::documentUrl() {
	return collection->collectionUrl() + "/" + name + ".document";
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