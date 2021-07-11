#ifndef FACEBOOK_AUTHORIZATION_H
#define FACEBOOK_AUTHORIZATION_H

#include "Authorization.h"

class FacebookProvider: AuthorizationProvider {
	string client_id;
	string client_secret;

	FacebookProvider(string client_id, string client_secret) {
		this->client_id = client_id;
		this->client_secret = client_secret;
	}
};

#endif