#define SERVER
#include <SwiftySyncServer.hpp>
#include <timercpp.h>

using namespace std;


bool isDataRequest(Request* request) {
    for (int i = 0; i < DATA_REQUEST_TYPES_COUNT; i++) {
        if (DATA_REQUEST_TYPES[i] == request->type) {
            return true;
        }
    }
    return false;
}

bool SecurityRule::checkAccess(Request* request) {
    if (isDataRequest(request)) {
        DataRequest* dataRequest = dynamic_cast<DataRequest*>(request);
        return dataRule(dataRequest);
    }
    else if (request->type == RequestType::function) {
        FunctionRequest* functionRequest = dynamic_cast<FunctionRequest*>(request);
        return functionRule(functionRequest);
    }
    return false;
}

Collection* SwiftyServer::operator [](string name) {
    for (int i = 0; i < collections.size(); i++) {
        if (name == collections[i].name) {
            return &collections[i];
        }
    }
    return NULL;
}

void SwiftyServer::read() {
    for (int i = 0; i < collections.size(); i++) {
        collections[i].read();
    }
}

void SwiftyServer::save() {
    for (auto collection : collections) {
        collection.save();
    }
}

void SwiftyServer::authorizeWithStatus(WebSocket ws, AuthorizationStatus status) {
    ConnectionData* data = (ConnectionData*)ws->getUserData();
    string respond = AUTH_PREFIX;
    if (status == AuthorizationStatus::authorized) {
        respond += AUTHORIZED_LOCALIZE;
        sockets[data->userId] = ws;
        ws->subscribe(data->userId);
    }
    else if (status == AuthorizationStatus::corruptedCredentials) {
        respond += CORR_CRED_LOCALIZE;
    }
    else if (status == AuthorizationStatus::error) {
        respond += AUTH_ERR_LOCALIZE;
    }
    ws->send(respond);
    behavior.authorized(status);
}

void SwiftyServer::authorize(WebSocket ws, string body) {
    ConnectionData* data = (ConnectionData*)ws->getUserData();
    ws->unsubscribe(data->userId);
    for (auto provider : supportedProviders) {
        if (provider->isValid(body)) {
            auto response = provider->authorize(body);
            if (response.status == AuthorizationStatus::authorized) {
                data->userId = response.userId;
            }
            authorizeWithStatus(ws, response.status);
            return;
        }
    }
    authorizeWithStatus(ws, AuthorizationStatus::corruptedCredentials);
}

void SwiftyServer::handleDataRequest(WebSocket ws, DataRequest* request) {
    string respond;
    respond += REQUEST_PREFIX;
    respond += DATA_REQUEST_PREFIX;
    respond += request->id;
    auto collection = operator[](request->collectionName);
    if (collection == nullptr) {
        ws->send(respond + DATA_REQUEST_FAILURE);
        return;
    }
    collection->createDocument(request->documentName);
    auto doc = collection->operator[](request->documentName);
    if (request->type == RequestType::documentGet) {
        JSONEncoder encoder;
        auto container = encoder.container();
        container.encode(doc->fields);
        respond += container.content;
    }
    if (request->type == RequestType::documentSet) {
        JSONDecoder decoder;
        auto container = decoder.container(request->body);
        auto fields = container.decode(vector<Field>());
        doc->fields = fields;
        respond += DATA_SET_SUCCESSFUL;
        doc->save();
    }
    if (request->type == RequestType::fieldGet) {
        JSONEncoder encoder;
        JSONDecoder decoder;
        auto decodeContainer = decoder.container(request->body);
        auto fieldRequest = decodeContainer.decode(FieldRequest());
        if (!fieldRequest.path.empty()) {
            Field* lastField = nullptr;
            for (int i = 0; i < doc->fields.size(); i++) {
                if (doc->fields[i].name == fieldRequest.path[0]) {
                    lastField = &doc->fields[i];
                }
            }
            for (int i = 1; i < fieldRequest.path.size(); i++) {
                if (lastField != nullptr) {
                    lastField = lastField->operator[](fieldRequest.path[i]);
                }
            }
            if (lastField != nullptr) {
                auto encodeContainer = encoder.container();
                //encodeContainer.encode(*lastField);
                respond += encodeContainer.content;
            }
        }
    }
    if (request->type == RequestType::fieldSet) {
        JSONDecoder decoder;
        auto container = decoder.container(request->body);
        auto fieldRequest = container.decode(FieldRequest());
        JSONDecoder valueDecoder;
        auto valueContainer = valueDecoder.container(fieldRequest.value);
        auto fieldValue = container.decode(Field());
        if (!fieldRequest.path.empty()) {
            Field* lastField = nullptr;
            for (int i = 0; i < doc->fields.size(); i++) {
                if (doc->fields[i].name == fieldRequest.path[0]) {
                    lastField = &doc->fields[i];
                }
            }
            for (int i = 1; i < fieldRequest.path.size(); i++) {
                if (lastField != nullptr) {
                    lastField = lastField->operator[](fieldRequest.path[i]);
                }
            }
            if (lastField != nullptr) {
                *lastField = fieldValue;
            }
        }
        respond += FIELD_SET_SUCCESSFUL;
        doc->save();
    }
    ws->send(respond);
}

void SwiftyServer::handleFunctionRequest(WebSocket ws, FunctionRequest* request) {
    for (int i = 0; i < functions.size(); i++) {
        if (functions[i].name == request->name) {
            auto output = functions[i][request->inputData];
            JSONEncoder encoder;
            auto container = encoder.container();
            container.encode(output);
            string respond = REQUEST_PREFIX;
            respond += FUNCTION_REQUEST_PREFIX;
            respond += request->id;
            respond += container.content;
            ws->send(respond);
            return;
        }
    }
}

void SwiftyServer::handleRequest(WebSocket ws, Request* request) {
    if (isDataRequest(request)) {
        if (rule.checkAccess(request)) {
            auto dataRequest = (DataRequest*) dynamic_cast<DataRequest*>(request);
            handleDataRequest(ws, dataRequest);
        }
        else {
            cout << "Access denied\n";
        }
        handlingRequest = false;
    }
    else if (request->type == RequestType::function) {
        if (rule.checkAccess(request)) {
            auto functionRequest = (FunctionRequest*) dynamic_cast<FunctionRequest*>(request);
            handleFunctionRequest(ws, functionRequest);
        }
        else {
            cout << "Access denied\n";
        }
        handlingRequest = false;
    }
}

Request* SwiftyServer::generateRequest(WebSocket ws, string body) {
    while (handlingRequest);
    handlingRequest = true;

    RequestType requestType = RequestType::undefined;

    ConnectionData* data = (ConnectionData*)ws->getUserData();
    JSONDecoder decoder;
    unsigned prefixSize;

    if (body.find(DOCUMENT_GET_PREFIX) == 0) {
        requestType = RequestType::documentGet;
        prefixSize = strlen(DOCUMENT_GET_PREFIX);
    }

    if (body.find(DOCUMENT_SET_PREFIX) == 0) {
        requestType = RequestType::documentSet;
        prefixSize = strlen(DOCUMENT_SET_PREFIX);
    }

    if (body.find(FIELD_GET_PREFIX) == 0) {
        requestType = RequestType::fieldGet;
        prefixSize = strlen(FIELD_GET_PREFIX);
    }

    if (body.find(FIELD_SET_PREFIX) == 0) {
        requestType = RequestType::fieldSet;
        prefixSize = strlen(FIELD_SET_PREFIX);
    }

    if (body.find(FUNCTION_REQUEST_PREFIX) == 0) {
        requestType = RequestType::function;
        prefixSize = strlen(FUNCTION_REQUEST_PREFIX);
        string encoded = body.substr(prefixSize, body.length() - prefixSize);
        auto container = decoder.container(encoded);
        auto functionResult = container.decode(FunctionRequest());
        functionResult.connection = data;
        functionResult.type = RequestType::function;
        lastFunctionRequest = functionResult;
        return &lastFunctionRequest;
    }

    Request tempRequest;
    tempRequest.type = requestType;

    if (isDataRequest(&tempRequest)) {
        string encoded = body.substr(prefixSize, body.length() - prefixSize);
        auto container = decoder.container(encoded);
        auto dataResult = container.decode(DataRequest());
        dataResult.connection = data;
        dataResult.type = requestType;
        lastDataRequest = dataResult;
        return &lastDataRequest;
    }

    return &tempRequest;
}

void SwiftyServer::handleMessage(WebSocket ws, string_view message) {
    ConnectionData* data = (ConnectionData*)ws->getUserData();
    string wrappedMessage = string(message);
    if (wrappedMessage.find(AUTH_PREFIX) == 0) {
        authorize(ws, wrappedMessage.substr(strlen(AUTH_PREFIX), wrappedMessage.length() - strlen(AUTH_PREFIX)));
    }
    else if (data->userId == "") {
        ws->send(string(AUTH_PREFIX) + string(AUTH_ERR_LOCALIZE));
        return;
    }
    if (wrappedMessage.find(REQUEST_PREFIX) == 0) {
        auto request = generateRequest(ws, wrappedMessage.substr(strlen(REQUEST_PREFIX), wrappedMessage.length() - strlen(REQUEST_PREFIX)));
        handleRequest(ws, request);
    }
}

void SwiftyServer::run(RunBehavior runBehavior) {
    bool started = true;
    read();
    save();
    Timer t = Timer();
    t.setInterval(runBehavior.update, runBehavior.updateInterval);
#ifdef SSL_SERVER
    auto key_file = runBehavior.key_filename.c_str();
    auto cert_file = runBehavior.cert_filename.c_str();
    auto phrase = runBehavior.passphrase.c_str();
    auto app = uWS::SSLApp({
        .key_file_name = key_file,
        .cert_file_name = cert_file,
        .passphrase = phrase
    });
    cout << "Using SSL\n";
#else
    auto app = uWS::App();
#endif
    app.ws<ConnectionData>("/*", {
        .open = [this](auto* ws) {
            ConnectionData* data = (ConnectionData*)ws->getUserData();
            data->connectionId = create_uuid();
            behavior.connectionOpened(ws);
        }, .message = [this](auto* ws, string_view message, uWS::OpCode opCode) {
            behavior.messageReceived(ws);
            handleMessage(ws, message);
        }, .close = [this](auto* ws, int code, string_view message) {
            behavior.connectionClosed(ws);
            ConnectionData* data = (ConnectionData*)ws->getUserData();
            ws->unsubscribe(data->userId);
        }
    }).listen(port, [this, runBehavior](auto* token) {
        behavior.completion(token);
        runBehavior.afterStart();
    }).run();
}

string Collection::collectionUrl() {
    return server->serverUrl + name + "/";
}

void Collection::save() {
    if (name.empty()) {
        return;
    }
    std::string url = collectionUrl();
    url.erase(url.end() - 1);
    fs::create_directory(url);
    for (auto document : documents) {
        document.save();
    }
}

std::string Document::documentUrl() {
    return collection->collectionUrl() + "/" + name + "." + DOCUMENT_EXTENSION;
}

void Document::read() {
    std::ifstream documentStream(documentUrl());
    std::string content;
    getline(documentStream, content);
    auto decoder = JSONDecoder();
    auto container = decoder.container(content);
    auto decoded = container.decode(std::vector<Field>());
    this->fields = decoded;
}

void Document::save() {
    std::ofstream documentStream(documentUrl());
    auto encoder = JSONEncoder();
    auto container = encoder.container();
    container.encode(fields);
    documentStream << container.content;
}

void Collection::read() {
    std::string url = collectionUrl();
    if (!fs::exists(url)) {
        std::cout << url << " doesn't exist\n";
        return;
    }
    for (auto& entry : fs::directory_iterator(url)) {
        std::string filename = entry.path().stem().string();
        createDocument(filename);
    }
    for (int i = 0; i < documents.size(); i++) {
        documents[i].read();
    }
}

void SwiftyServer::sendData(string userId, DataUnit data) {
    string message;
    for(int i=0;i<data.bytes.size();i++) {
        message += data.bytes[i];
    }
    sockets[userId]->publish(userId, message);
}