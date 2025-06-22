#include "test_server.h"

int TestServer::get_value() const {
    return value;
}

void TestServer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_value"), &TestServer::get_value);
}

namespace Global {
::TestServer& TestServer() {
    static ::TestServer test_server;
    return test_server;
}
} // namespace Global
