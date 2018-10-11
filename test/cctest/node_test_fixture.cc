#include "node_test_fixture.h"

ArrayBufferUniquePtr NodeTestFixture::allocator{nullptr, nullptr};
uv_loop_t NodeTestFixture::current_loop;
NodePlatformUniquePtr NodeTestFixture::platform;
TracingAgentUniquePtr NodeTestFixture::tracing_agent;
