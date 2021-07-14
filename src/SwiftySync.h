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
			cout << "Document is already created\n";
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

	Collection* operator [](string name) {
		for (int i = 0; i < collections.size(); i++) {
			if (name == collections[i].name) {
				return &collections[i];
			}
		}
		return NULL;
	}

	void read() {
		string listPath = "collectionList.txt";
		if (!fs::exists(listPath)) {
			return;
		}
		ifstream list(listPath);
		vector<Collection> newCollections;
		while (!list.eof()) {
			string collectionName;
			list >> collectionName;
			newCollections.push_back(Collection(this, collectionName));
		}
		collections = newCollections;
		for (int i = 0; i < collections.size(); i++) {
			collections[i].read();
		}
	}

	void save() {
		for (auto collection : collections) {
			collection.save();
		}
		ofstream list("collectionList.txt");
		for (auto collection : collections) {
			list << collection.name << "\n";
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
	string url = collectionUrl();
	url.erase(url.end() - 1);
	fs::create_directory(url);
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