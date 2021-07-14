#include "SwiftySync.h"
#include <iostream>
#include "Authorization.h"
#include "GoogleAuthorization.h"
#include "FacebookAuthorization.h"

#ifndef GOOGLE_CLIENT_ID
#define GOOGLE_CLIENT_ID "your-client-id"
#endif

#ifndef FACEBOOK_CLIENT_ID
#define FACEBOOK_CLIENT_ID ""
#define FACEBOOK_CLIENT_SECRET ""
#endif

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
		Collection(&server, "trips")
	};

	auto googleProvider = GoogleProvider(GOOGLE_CLIENT_ID);
	auto castedGoogleProvider = (AuthorizationProvider*) static_cast<AuthorizationProvider*>(&googleProvider);
	
	auto facebookProvider = FacebookProvider(FACEBOOK_CLIENT_ID, FACEBOOK_CLIENT_SECRET);
	auto castedFacebookProvider = (FacebookProvider*) static_cast<FacebookProvider*>(&facebookProvider);

	server.supportedProviders = {
		castedGoogleProvider,
		castedFacebookProvider
	};
	server.run();
	if (server["users"] == NULL) {
		cout << "There is no USERS container\n";
	}
	else {
		server["users"]->createDocument("stefjen07");
	}
	return 0;
}