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

	GoogleProvider() {
		Py_Initialize();
		authScript = fopen(GOOGLE_AUTH_SCRIPT_NAME, "r");
	}

	~GoogleProvider() {
		Py_Finalize();
	}
};

#endif