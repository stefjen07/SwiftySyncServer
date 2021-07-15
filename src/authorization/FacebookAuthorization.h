#ifndef FACEBOOK_AUTHORIZATION_H
#define FACEBOOK_AUTHORIZATION_H

#include "Authorization.h"
#include <string>
#include <iostream>

#define FACEBOOK_AUTH_PREFIX "F"

using namespace std;

class FacebookResponse : public Codable {
public:
	long long app_id, user_id, expires_at;

	void encode(CoderContainer* container) {
		if (container->type == CoderType::json) {
			JSONEncodeContainer* jsonContainer = dynamic_cast<JSONEncodeContainer*>(container);
			jsonContainer->encode(app_id, "app_id");
			jsonContainer->encode(user_id, "user_id");
			jsonContainer->encode(expires_at, "expires_at");
		}
	}

	void decode(CoderContainer* container) {
		if (container->type == CoderType::json) {
			JSONDecodeContainer* jsonContainer = dynamic_cast<JSONDecodeContainer*>(container);
			app_id = jsonContainer->decode(0LL, "app_id");
			user_id = jsonContainer->decode(0LL, "user_id");
			expires_at = jsonContainer->decode(0LL, "expires_at");
		}
	}
};

class FacebookProvider: public AuthorizationProvider {
public:
	string access_token;
	string app_id;

	AuthorizationResponse authorize(string body) {
		string input_token = body;
		AuthorizationResponse result;
		result.status = AuthorizationStatus::error;
		httplib::SSLClient cli("https://graph.facebook.com");
		httplib::Params params;
		params.emplace("input_token", input_token);
		params.emplace("access_token", access_token);

		auto response = cli.Post("/debug_token", params);

		if (response == nullptr) {
			#ifdef AUTH_DEBUG
			cout << "There is no response for request\n";
			#endif
		}
		else {
			if (response->status == 200) {
				JSONDecoder decoder;
				auto container = decoder.container(response->body);
				const auto decoded = container.decode(FacebookResponse(), "data");
				if (to_string(decoded.app_id) == app_id) {
					result.status = AuthorizationStatus::authorized;
					result.userId = FACEBOOK_AUTH_PREFIX + to_string(decoded.user_id);
				}
			}
			else {
				#ifdef AUTH_DEBUG
				cout << "Request returned " << response->status << "\n";
				#endif
			}
		}

		return result;
	}

	bool isValid(string body) {
		return body.find(FACEBOOK_AUTH_PREFIX) == 0;
	}

	FacebookProvider(string access_token, string app_id) {
		this->access_token = access_token;
		this->app_id = app_id;
	}
};

#endif