#include "test_server.h"

TestServer* TestServer::singleton = nullptr;

TestServer::TestServer() {
    singleton = this;
}

TestServer::~TestServer() {
    singleton = nullptr;
}

TestServer* TestServer::get_singleton() {
    return singleton;
}

int TestServer::get_value() const {
    return value;
}

void TestServer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_value"), &TestServer::get_value);
}
