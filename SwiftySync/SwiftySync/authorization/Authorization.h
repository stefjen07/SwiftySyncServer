#ifndef AUTHORIZATION_H
#define AUTHORIZATION_H

enum class AuthorizationStatus {
	authorized,
	error
};

class AuthorizationProvider {
	virtual AuthorizationStatus authorize(string body) = 0;
};

#endif