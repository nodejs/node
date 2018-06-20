#include "node_test_fixture.h"

ArrayBufferUniquePtr NodeTestFixture::allocator{nullptr, nullptr};
uv_loop_t NodeTestFixture::current_loop;
std::unique_ptr<node::NodePlatform> NodeTestFixture::platform;
std::unique_ptr<v8::TracingController> NodeTestFixture::tracing_controller;
