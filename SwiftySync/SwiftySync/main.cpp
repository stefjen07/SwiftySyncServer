#include "SwiftySync.h"
#include <iostream>
#include "Authorization.h"
#include "GoogleAuthorization.h"

int main() {
	int port = 8888;
	SwiftyServer server("localhost", port);
	server.collections = {
		Collection(&server, "users"),
		Collection(&server, "trips")
	};
	auto googleProvider = GoogleProvider();
	auto castedGoogleProvider = (AuthorizationProvider*) static_cast<AuthorizationProvider*>(&googleProvider);
	server.supportedProviders = {
		castedGoogleProvider
	};
	server.run([port](bool started) {
		if (started) {
			cout << "Server started on port " << port << endl;
		}
		else {
			cout << "Start failed" << endl;
		}
	}, [](auto* ws) {
		auto connectionData = (ConnectionData*)ws->getUserData();
		cout << "New connection with id " << connectionData->connectionId << "\n";
	}, [](auto* ws) {
		auto connectionData = (ConnectionData*)ws->getUserData();
		cout << "Message received from id " << connectionData->connectionId << "\n";
	}, []() {
		cout << "Authorized\n";
	});
	if (server["users"] == NULL) {
		cout << "There is no USERS container\n";
	}
	else {
		server["users"]->createDocument("stefjen07");
	}
	return 0;
}