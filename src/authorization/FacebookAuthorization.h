#ifndef FACEBOOK_AUTHORIZATION_H
#define FACEBOOK_AUTHORIZATION_H

#include "Authorization.h"
#include <string>

using namespace std;

class FacebookProvider: public AuthorizationProvider {
public:
	string client_id;
	string client_secret;

	AuthorizationResponse authorize(string body) {
		AuthorizationResponse result;
		result.status = AuthorizationStatus::error;
		return result;
	}

	bool isValid(string body) {
		char type = body[0];
		return type == 'F';
	}

	FacebookProvider(string client_id, string client_secret) {
		this->client_id = client_id;
		this->client_secret = client_secret;
	}
};

#endif