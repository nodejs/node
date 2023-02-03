#include "node_test_fixture.h"
#include "cppgc/platform.h"

ArrayBufferUniquePtr NodeZeroIsolateTestFixture::allocator{nullptr, nullptr};
uv_loop_t NodeZeroIsolateTestFixture::current_loop;
NodePlatformUniquePtr NodeZeroIsolateTestFixture::platform;
TracingAgentUniquePtr NodeZeroIsolateTestFixture::tracing_agent;
bool NodeZeroIsolateTestFixture::node_initialized = false;


void NodeTestEnvironment::SetUp() {
  NodeZeroIsolateTestFixture::tracing_agent =
      std::make_unique<node::tracing::Agent>();
  node::tracing::TraceEventHelper::SetAgent(
      NodeZeroIsolateTestFixture::tracing_agent.get());
  node::tracing::TracingController* tracing_controller =
      NodeZeroIsolateTestFixture::tracing_agent->GetTracingController();
  static constexpr int kV8ThreadPoolSize = 4;
  NodeZeroIsolateTestFixture::platform.reset(
      new node::NodePlatform(kV8ThreadPoolSize, tracing_controller));
  v8::V8::InitializePlatform(NodeZeroIsolateTestFixture::platform.get());
#ifdef V8_ENABLE_SANDBOX
  ASSERT_TRUE(v8::V8::InitializeSandbox());
#endif
  cppgc::InitializeProcess(
      NodeZeroIsolateTestFixture::platform->GetPageAllocator());

  // Before initializing V8, disable the --freeze-flags-after-init flag, so
  // individual tests can set their own flags.
  v8::V8::SetFlagsFromString("--no-freeze-flags-after-init");

  v8::V8::Initialize();
}

void NodeTestEnvironment::TearDown() {
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  NodeZeroIsolateTestFixture::platform->Shutdown();
  NodeZeroIsolateTestFixture::platform.reset(nullptr);
  NodeZeroIsolateTestFixture::tracing_agent.reset(nullptr);
}

::testing::Environment* const node_env =
::testing::AddGlobalTestEnvironment(new NodeTestEnvironment());
