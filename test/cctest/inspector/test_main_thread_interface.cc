#include "gtest/gtest.h"
#include "inspector/main_thread_interface.h"
#include "inspector/worker_inspector.h"
#include "node_test_fixture.h"

#include <memory>

namespace node {
namespace inspector {
namespace {

class MainThreadInterfaceTest : public EnvironmentTestFixture {};

class RetainInterfaceRequest : public Request {
 public:
  explicit RetainInterfaceRequest(
      std::shared_ptr<MainThreadInterface>* retained_interface)
      : retained_interface_(retained_interface) {}

  void Call(MainThreadInterface* thread) override {
    *retained_interface_ = thread->shared_from_this();
  }

 private:
  std::shared_ptr<MainThreadInterface>* retained_interface_;
};

TEST_F(MainThreadInterfaceTest, ClosesHandleBeforeReleasingInterface) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  std::shared_ptr<MainThreadInterface> retained_interface;
  std::shared_ptr<MainThreadHandle> handle;
  {
    Env env{handle_scope, argv};
    handle = (*env)->inspector_agent()->GetWorkerManager()->MainThread();
    ASSERT_FALSE(handle->Expired());
    ASSERT_TRUE(handle->Post(
        std::make_unique<RetainInterfaceRequest>(&retained_interface)));
    (*env)->RunAndClearInterrupts();
    ASSERT_NE(retained_interface, nullptr);

    // Keep the interface alive after its inspector owner is destroyed, so
    // closing the handle cannot depend on the interface's destructor.
  }

  ASSERT_TRUE(handle->Expired());
  EXPECT_FALSE(handle->Post(
      std::make_unique<RetainInterfaceRequest>(&retained_interface)));
  retained_interface.reset();
}

}  // namespace
}  // namespace inspector
}  // namespace node
