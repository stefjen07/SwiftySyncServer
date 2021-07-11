#ifndef HELPER_H
#define HELPER_H

string create_uuid() {
    const char* v = "0123456789abcdef";
    const bool dash[] = { 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0 };

    string res;
    for (int i = 0; i < 16; i++) {
        if (dash[i]) res += "-";
        res += v[rand() % 16];
        res += v[rand() % 16];
    }
    return res;
}

#endif