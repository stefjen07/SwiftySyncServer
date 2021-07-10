#include "SwiftySync.h"
#include <iostream>

int main() {
	int port = 8888;
	SwiftyServer server("localhost", port);
	server.collections = {
		Collection(&server, "users"),
		Collection(&server, "trips")
	};
	server.run([port](bool started) {
		if (started) {
			cout << "Server started on port " << port << endl;
		}
		else {
			cout << "Start failed" << endl;
		}
	});
	if (server["users"] == NULL) {
		cout << "There is no USERS container\n";
	}
	else {
		server["users"]->createDocument("stefjen07");
	}
	return 0;
}