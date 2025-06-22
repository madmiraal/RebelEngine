#ifndef TEST_SERVER_H
#define TEST_SERVER_H

#include "core/object.h"

class TestServer : public Object {
    GDCLASS(TestServer, Object);

public:
    int get_value() const;

private:
    static void _bind_methods();

    int value = 10;
};

namespace Global {
::TestServer& TestServer();
} // namespace Global

#endif // TEST_SERVER_H
