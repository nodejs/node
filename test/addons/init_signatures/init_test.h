#ifndef NODE_TEST_ADDON_INIT_TEST_H
# define NODE_TEST_ADDON_INIT_TEST_H

#include <node.h>

namespace node { namespace test {

inline
void
set(v8::Handle<v8::Object> obj, char const* name, bool value) {
    v8::Isolate * isolate = v8::Isolate::GetCurrent();
    obj->Set(
        v8::String::NewFromUtf8(isolate, name),
        v8::Boolean::New(isolate, value));
}

inline
void
setInitTag(v8::Handle<v8::Object> obj) {
  set(obj, "initialized", true);
}

}}  // end of namespace node::test
#endif // NODE_TEST_ADDON_INIT_TEST_H
