#include "node_test_fixture.h"

uv_loop_t NodeTestFixture::current_loop;
std::unique_ptr<node::NodePlatform> NodeTestFixture::platform;
std::unique_ptr<v8::ArrayBuffer::Allocator> NodeTestFixture::allocator;
std::unique_ptr<v8::TracingController> NodeTestFixture::tracing_controller;
v8::Isolate::CreateParams NodeTestFixture::params;
