#ifndef GOOGLE_AUTHORIZATION_H
#define GOOGLE_AUTHORIZATION_H

#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "Authorization.h"

#define GOOGLE_AUTH_SCRIPT_NAME "googleAuth.py"

extern "C"
class GoogleProvider: public AuthorizationProvider {
public:
	FILE* authScript;

	AuthorizationStatus authorize(string body) {
		PyRun_SimpleFile(authScript, GOOGLE_AUTH_SCRIPT_NAME);
		return AuthorizationStatus::error;
	}

	bool isValid(string body) {
		return false;
	}

	GoogleProvider() {
		Py_Initialize();
		fopen_s(&authScript, GOOGLE_AUTH_SCRIPT_NAME, "r");
	}

	~GoogleProvider() {
		Py_Finalize();
	}
};

#endif