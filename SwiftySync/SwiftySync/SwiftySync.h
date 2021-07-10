#ifndef SWIFTYSYNC_H
#define SWIFTYSYNC_H

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING

#if (__cplusplus >= 201703L)
#include <filesystem>
#else
#include <experimental/filesystem>
#endif
#include <vector>
#include <string>
#include <functional>
#include <fstream>

namespace fs = std::experimental::filesystem;
using namespace std;

enum class FieldType {

};

class Field {
public:
	FieldType type;
};

class Collection;

class Document {
public:
	string name;
	vector<Field> fields;
	Collection* collection;

	string documentUrl();

	void save();

	Document(Collection* collection, string name) {
		this->name = name;
		this->collection = collection;
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

class SwiftyServer {
public:
	string address;
	unsigned port;
	string serverUrl = "";

	vector<Collection> collections;

	Collection* operator [](string name) {
		for (int i = 0; i < collections.size(); i++) {
			if (name == collections[i].name) {
				return &collections[i];
			}
		}
		return NULL;
	}

	void save() {
		for (auto collection : collections) {
			collection.save();
		}
	}

	void run(const function<void(bool)> completion) {
		bool started = true;
		save();
		completion(started);
	}

	SwiftyServer(string address, int port) {
		this->address = address;
		this->port = port;
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

void Document::save() {
	ofstream documentStream(documentUrl());
	for (auto field : fields) {

	}
}

#endif