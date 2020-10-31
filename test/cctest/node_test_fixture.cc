#include "node_test_fixture.h"

ArrayBufferUniquePtr NodeZeroIsolateTestFixture::allocator{nullptr, nullptr};
uv_loop_t NodeZeroIsolateTestFixture::current_loop;
NodePlatformUniquePtr NodeZeroIsolateTestFixture::platform;

#ifndef V8_USE_PERFETTO
TracingAgentUniquePtr NodeZeroIsolateTestFixture::tracing_agent;
#endif

bool NodeZeroIsolateTestFixture::node_initialized = false;
