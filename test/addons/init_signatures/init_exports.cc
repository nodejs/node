#include "init_test.h"
void init(v8::Local<v8::Object> exports) {
  node::test::setInitTag(exports);
}
NODE_MODULE(NODE_TEST_ADDON_NAME, init)
