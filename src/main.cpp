#include "SwiftySync.h"
#include <iostream>
#include "Authorization.h"
#include "GoogleAuthorization.h"
#include "FacebookAuthorization.h"

#ifndef GOOGLE_CLIENT_ID
#define GOOGLE_CLIENT_ID "your-client-id"
#endif

#ifndef FACEBOOK_ACCESS_TOKEN
#define FACEBOOK_ACCESS_TOKEN "your-access-token"
#define FACEBOOK_APP_ID "your-app-id"
#endif

class DebugProvider : public AuthorizationProvider {
	AuthorizationResponse authorize(string body) {
		AuthorizationResponse response;
		response.status = AuthorizationStatus::authorized;
		response.userId = "stefjen07";
		return response;
	}
	bool isValid(string body) {
		return true;
	}
};

int main() {
	int port = 8888;
	SwiftyServer server("localhost", port, {
		.completion = [port](bool started) {
			if (started) {
				cout << "Server started on port " << port << endl;
			}
			else {
				cout << "Start failed" << endl;
			}
		},
		.connectionOpened = [](auto* ws) {
			auto connectionData = (ConnectionData*)ws->getUserData();
			cout << "New connection with id " << connectionData->connectionId << "\n";
		},
		.messageReceived = [](auto* ws) {
			auto connectionData = (ConnectionData*)ws->getUserData();
			cout << "Message received from id " << connectionData->connectionId << "\n";
		},
		.authorized = [](AuthorizationStatus status) {
			if (status == AuthorizationStatus::authorized) {
				cout << "Authorized\n";
			}
			else {
				cout << "Authorization failed\n";
			}
		}
	});
	server.collections = {
		Collection(&server, "users"),
		Collection(&server, "trips"),
		Collection(&server, "privileges")
	};

	auto googleProvider = GoogleProvider(GOOGLE_CLIENT_ID);
	auto castedGoogleProvider = (AuthorizationProvider*) static_cast<AuthorizationProvider*>(&googleProvider);
	
	auto facebookProvider = FacebookProvider(FACEBOOK_ACCESS_TOKEN, FACEBOOK_APP_ID);
	auto castedFacebookProvider = (FacebookProvider*) static_cast<FacebookProvider*>(&facebookProvider);

	auto debugProvider = DebugProvider();
	auto castedDebugProvider = (DebugProvider*) static_cast<DebugProvider*>(&debugProvider);

	server.supportedProviders = {
		castedGoogleProvider,
		castedFacebookProvider,
		castedDebugProvider
	};
	Collection* usersCollection = server["users"];
	Collection* tripsCollection = server["trips"];
	Collection* privilegesCollection = server["privileges"];
	usersCollection->onDocumentCreating = []() {
		cout << "New document created\n";
	};
	server.rule = {
		.dataRule = [usersCollection, tripsCollection, privilegesCollection](DataRequest* request) {
			Collection* requestCollection = nullptr;
			if (request->collectionName == "users")
				requestCollection = usersCollection;
			if (request->collectionName == "trips")
				requestCollection = tripsCollection;
			if (request->collectionName == "privileges")
				requestCollection = privilegesCollection;
			if (requestCollection == nullptr)
				return false;
			if (requestCollection == usersCollection) {
				return requestCollection->isDocumentNameTaken(request->documentName) && request->documentName == request->connection.userId;
			}
			if (requestCollection == tripsCollection || requestCollection == privilegesCollection) {
				if (request->type == RequestType::documentGet) {
					if (!tripsCollection->isDocumentNameTaken(request->documentName) || !privilegesCollection->isDocumentNameTaken(request->documentName)) {
						return false;
					}
					auto privilegesDoc = privilegesCollection->operator[](request->documentName);
					auto members = privilegesDoc->operator[]("members");
					if (members != NULL) {
						for (auto child : members->children) {
							if (child.strValue == request->connection.userId) {
								return true;
							}
						}
					}
				}
				if (request->type == RequestType::documentSet) {
					if (!requestCollection->isDocumentNameTaken(request->documentName)) {
						return true;
					}
					else {
						auto privilegesDoc = privilegesCollection->operator[](request->documentName);
						if (privilegesDoc != NULL) {
							auto admin = privilegesDoc->operator[]("admin");
							if (admin != NULL) {
								if (admin->strValue == request->connection.userId) {
									return true;
								}
							}
						}
					}
				}
			}
			return false;
		}
	};
	server.run({
		.afterStart = [usersCollection]() {
			if (usersCollection == NULL) {
				cout << "There is no USERS container\n";
			}
			else {
				usersCollection->createDocument("stefjen07");
			}
		},
		.update = [server]() {
			cout << "Everything is OK\n";
		},
		.updateInterval = 10000
	});
	return 0;
}