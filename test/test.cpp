#define SERVER
#define SSL_SERVER

#include <SwiftySyncServer.hpp>
#include <iostream>
#include <Authorization.hpp>
#include <GoogleAuthorization.hpp>
#include <FacebookAuthorization.hpp>
#include <SwiftySyncStorage.hpp>
#include <Functions.hpp>

//#define CHECK_FOR_PRIVILEGES

#ifndef GOOGLE_CLIENT_ID
#define GOOGLE_CLIENT_ID "your-client-id"
#endif

#ifndef FACEBOOK_ACCESS_TOKEN
#define FACEBOOK_ACCESS_TOKEN "your-access-token"
#define FACEBOOK_APP_ID "your-app-id"
#endif

class DebugProvider : public AuthorizationProvider {
	AuthorizationResponse authorize(std::string body) {
		AuthorizationResponse response;
		response.status = AuthorizationStatus::authorized;
		response.userId = "stefjen07";
		return response;
	}
	bool isValid(std::string body) {
		return true;
	}
};

int main() {
	int port = 8888;
	SwiftyServer server("localhost", port, {
		.completion = [port](bool started) {
			if (started) {
				std::cout << "Server started on port " << port << std::endl;
			}
			else {
				std::cout << "Start failed" << std::endl;
			}
		},
		.connectionOpened = [](auto* ws) {
			auto connectionData = (ConnectionData*)ws->getUserData();
			std::cout << "New connection with id " << connectionData->connectionId << "\n";
		},
		.messageReceived = [](auto* ws) {
			auto connectionData = (ConnectionData*)ws->getUserData();
			std::cout << "Message received from id " << connectionData->connectionId << "\n";
		},
		.authorized = [](AuthorizationStatus status) {
			if (status == AuthorizationStatus::authorized) {
				std::cout << "Authorized\n";
			}
			else {
				std::cout << "Authorization failed\n";
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

	server.functions = {
		Function("nothing", [](DataUnit input) {
			return input;
		})
	};

	Collection* usersCollection = server["users"];
	Collection* tripsCollection = server["trips"];
	Collection* privilegesCollection = server["privileges"];
	usersCollection->onDocumentCreating = []() {
		std::cout << "New document created\n";
	};
	server.rule = {
		.dataRule = [usersCollection, tripsCollection, privilegesCollection](DataRequest* request) {
#ifndef CHECK_FOR_PRIVILEGES
			return true;
#endif
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
				return requestCollection->isDocumentNameTaken(request->documentName) && request->documentName == request->connection->userId;
			}
			if (requestCollection == tripsCollection || requestCollection == privilegesCollection) {
				if (request->type == RequestType::documentGet) {
					if (!tripsCollection->isDocumentNameTaken(request->documentName) || !privilegesCollection->isDocumentNameTaken(request->documentName)) {
						return false;
					}
					#ifndef CHECK_FOR_PRIVILEGES
					return true;
					#endif
					auto privilegesDoc = privilegesCollection->operator[](request->documentName);
					auto members = privilegesDoc->operator[]("members");
					if (members != NULL) {
						for (auto child : members->children) {
							if (child.strValue == request->connection->userId) {
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
						#ifndef CHECK_FOR_PRIVILEGES
						return true;
						#endif
						auto privilegesDoc = privilegesCollection->operator[](request->documentName);
						if (privilegesDoc != NULL) {
							auto admin = privilegesDoc->operator[]("admin");
							if (admin != NULL) {
								if (admin->strValue == request->connection->userId) {
									return true;
								}
							}
						}
					}
				}
			}
			return false;
		},
		.functionRule = [](FunctionRequest* request) {
			return true;
		}
	};
	server.run({
		.afterStart = [usersCollection]() {
			if (usersCollection == NULL) {
				std::cout << "There is no USERS container\n";
			}
			else {
				usersCollection->createDocument("stefjen07");
			}
		},
		.update = [server]() {

			std::cout << "Everything is OK\n";
		},
		.updateInterval = 10000,
		.key_filename = "certificate-private-key.pem",
		.cert_filename = "certificate.pem",
		.passphrase = "TEST"
	});
	return 0;
}