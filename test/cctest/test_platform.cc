#include "node_internals.h"
#include "libplatform/libplatform.h"

#include <string>
#include "gtest/gtest.h"
#include "node_test_fixture.h"

// This task increments the given run counter and reposts itself until the
// repost counter reaches zero.
class RepostingTask : public v8::Task {
 public:
  explicit RepostingTask(int repost_count,
                         int* run_count,
                         v8::Isolate* isolate,
                         node::NodePlatform* platform)
      : repost_count_(repost_count),
        run_count_(run_count),
        isolate_(isolate),
        platform_(platform) {}

  // v8::Task implementation
  void Run() final {
    ++*run_count_;
    if (repost_count_ > 0) {
      --repost_count_;
      platform_->CallOnForegroundThread(isolate_,
          new RepostingTask(repost_count_, run_count_, isolate_, platform_));
    }
  }

 private:
  int repost_count_;
  int* run_count_;
  v8::Isolate* isolate_;
  node::NodePlatform* platform_;
};

class PlatformTest : public EnvironmentTestFixture {};

TEST_F(PlatformTest, SkipNewTasksInFlushForegroundTasks) {
  v8::Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env {handle_scope, argv};
  int run_count = 0;
  platform->CallOnForegroundThread(
      isolate_, new RepostingTask(2, &run_count, isolate_, platform.get()));
  EXPECT_TRUE(platform->FlushForegroundTasks(isolate_));
  EXPECT_EQ(1, run_count);
  EXPECT_TRUE(platform->FlushForegroundTasks(isolate_));
  EXPECT_EQ(2, run_count);
  EXPECT_TRUE(platform->FlushForegroundTasks(isolate_));
  EXPECT_EQ(3, run_count);
  EXPECT_FALSE(platform->FlushForegroundTasks(isolate_));
}
