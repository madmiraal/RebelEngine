#ifndef TEST_SERVER_H
#define TEST_SERVER_H

#include "core/object.h"

class TestServer : public Object {
    GDCLASS(TestServer, Object);

public:
    TestServer();
    ~TestServer();

    static TestServer* get_singleton();

    int get_value() const;

private:
    static TestServer* singleton;
    static void _bind_methods();

    int value = 10;
};

#endif // TEST_SERVER_H
