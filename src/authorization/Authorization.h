#ifndef AUTHORIZATION_H
#define AUTHORIZATION_H

enum class AuthorizationStatus {
	authorized,
	corruptedCredentials,
	error
};

class AuthorizationResponse {
public:
	AuthorizationStatus status;
	string userId;

	AuthorizationResponse(AuthorizationStatus status, string userId) {
		this->status = status;
		this->userId = userId;
	}

	AuthorizationResponse() {

	}
};

class AuthorizationProvider {
public:
	virtual AuthorizationResponse authorize(string body) = 0;
	virtual bool isValid(string body) = 0;
};

#endif