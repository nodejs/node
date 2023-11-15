#include "node.h"

#include <string>
#include "gtest/gtest.h"
#include "node_test_fixture.h"

using node::Environment;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::SealHandleScope;
using v8::String;
using v8::Value;


class DotEnvTest : public EnvironmentTestFixture {
 private:
  void TearDown() override {

  }
};

TEST_F(DotEnvTest, ReadDotEnvFile) {


}
