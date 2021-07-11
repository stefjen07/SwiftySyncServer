#ifndef AUTHORIZATION_H
#define AUTHORIZATION_H

enum class AuthorizationStatus {
	authorized,
	corruptedCredentials,
	error
};

class AuthorizationProvider {
public:
	virtual AuthorizationStatus authorize(string body) = 0;
	virtual bool isValid(string body) = 0;
};

#endif