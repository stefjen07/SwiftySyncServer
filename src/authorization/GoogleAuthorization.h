#ifndef GOOGLE_AUTHORIZATION_H 
#define GOOGLE_AUTHORIZATION_H

#define CPPHTTPLIB_OPENSSL_SUPPORT

#include "httplib.h"
#include "Authorization.h"
#include <string>
#include <iostream>
#define GOOGLE_AUTH_SCRIPT_NAME "googleAuth"
#define GOOGLE_AUTH_FUNC "authorize"
using namespace std;

class GoogleResponse : public Codable {
public:
	string iss, sub, azp, aud, iat, exp;

	void encode(CoderContainer* container) {
		if (container->type == CoderType::json) {
			JSONEncodeContainer* jsonContainer = dynamic_cast<JSONEncodeContainer*>(container);
			jsonContainer->encode(iss, "iss");
			jsonContainer->encode(sub, "sub");
			jsonContainer->encode(azp, "azp");
			jsonContainer->encode(aud, "aud");
			jsonContainer->encode(iat, "iat");
			jsonContainer->encode(exp, "exp");
		}
	}

	void decode(CoderContainer* container) {
		if (container->type == CoderType::json) {
			JSONDecodeContainer* jsonContainer = dynamic_cast<JSONDecodeContainer*>(container);
			iss = jsonContainer->decode(string(), "iss");
			sub = jsonContainer->decode(string(), "sub");
			azp = jsonContainer->decode(string(), "azp");
			aud = jsonContainer->decode(string(), "aud");
			iat = jsonContainer->decode(string(), "iat");
			exp = jsonContainer->decode(string(), "exp");
		}
	}
};

class GoogleProvider: public AuthorizationProvider {
public:
	string client_id;

	AuthorizationResponse authorize(string body) {
		AuthorizationResponse result;
		result.status = AuthorizationStatus::error;
		string token = body;
		httplib::SSLClient cli("https://oauth2.googleapis.com");
		httplib::Params params;
		params.emplace("id_token", token);

		auto response = cli.Post("/tokeninfo", params);
		
		if (response == nullptr) {
			cout << "There is no response for request\n";
		}
		else {
			if (response->status == 200) {
				JSONDecoder decoder;
				auto container = decoder.container(response->body);
				const auto decoded = container.decode(GoogleResponse());
				if (decoded.aud == client_id) {
					result.status = AuthorizationStatus::authorized;
					result.userId = "G" + decoded.sub;
				}
			}
			else {
				cout << "Request returned " << response->status << "\n";
			}
		}

		return result;
	}

	bool isValid(string body) {
		char type = body[0];
		return type == 'G';
	}

	GoogleProvider(string client_id) {
		this->client_id = client_id;
	}
};

#endif