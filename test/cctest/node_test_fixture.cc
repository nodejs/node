#include <stdlib.h>
#include "node_test_fixture.h"

v8::Isolate::CreateParams NodeTestFixture::params_;
ArrayBufferAllocator NodeTestFixture::allocator_;
v8::Platform* NodeTestFixture::platform_;
uv_loop_t NodeTestFixture::current_loop;
