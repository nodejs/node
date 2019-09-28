#include "gtest/gtest.h"
#include "node.h"
#include "base_object-inl.h"
#include "node_test_fixture.h"

using node::BaseObject;
using node::BaseObjectPtr;
using node::BaseObjectWeakPtr;
using node::Environment;
using node::MakeBaseObject;
using node::MakeDetachedBaseObject;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;

class BaseObjectPtrTest : public EnvironmentTestFixture {};

class DummyBaseObject : public BaseObject {
 public:
  DummyBaseObject(Environment* env, Local<Object> obj) : BaseObject(env, obj) {}

  static Local<Object> MakeJSObject(Environment* env) {
    return BaseObject::MakeLazilyInitializedJSTemplate(env)
        ->GetFunction(env->context()).ToLocalChecked()
        ->NewInstance(env->context()).ToLocalChecked();
  }

  static BaseObjectPtr<DummyBaseObject> NewDetached(Environment* env) {
    Local<Object> obj = MakeJSObject(env);
    return MakeDetachedBaseObject<DummyBaseObject>(env, obj);
  }

  static BaseObjectPtr<DummyBaseObject> New(Environment* env) {
    Local<Object> obj = MakeJSObject(env);
    return MakeBaseObject<DummyBaseObject>(env, obj);
  }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(DummyBaseObject)
  SET_SELF_SIZE(DummyBaseObject)
};

TEST_F(BaseObjectPtrTest, ScopedDetached) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env_{handle_scope, argv};
  Environment* env = *env_;

  EXPECT_EQ(env->base_object_count(), 0);
  {
    BaseObjectPtr<DummyBaseObject> ptr = DummyBaseObject::NewDetached(env);
    EXPECT_EQ(env->base_object_count(), 1);
  }
  EXPECT_EQ(env->base_object_count(), 0);
}

TEST_F(BaseObjectPtrTest, ScopedDetachedWithWeak) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env_{handle_scope, argv};
  Environment* env = *env_;

  BaseObjectWeakPtr<DummyBaseObject> weak_ptr;

  EXPECT_EQ(env->base_object_count(), 0);
  {
    BaseObjectPtr<DummyBaseObject> ptr = DummyBaseObject::NewDetached(env);
    weak_ptr = ptr;
    EXPECT_EQ(env->base_object_count(), 1);
  }
  EXPECT_EQ(weak_ptr.get(), nullptr);
  EXPECT_EQ(env->base_object_count(), 0);
}

TEST_F(BaseObjectPtrTest, Undetached) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env_{handle_scope, argv};
  Environment* env = *env_;

  node::AddEnvironmentCleanupHook(isolate_, [](void* arg) {
    EXPECT_EQ(static_cast<Environment*>(arg)->base_object_count(), 0);
  }, env);

  BaseObjectPtr<DummyBaseObject> ptr = DummyBaseObject::New(env);
  EXPECT_EQ(env->base_object_count(), 1);
}

TEST_F(BaseObjectPtrTest, GCWeak) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env_{handle_scope, argv};
  Environment* env = *env_;

  BaseObjectWeakPtr<DummyBaseObject> weak_ptr;

  {
    const HandleScope handle_scope(isolate_);
    BaseObjectPtr<DummyBaseObject> ptr = DummyBaseObject::New(env);
    weak_ptr = ptr;
    ptr->MakeWeak();

    EXPECT_EQ(env->base_object_count(), 1);
    EXPECT_EQ(weak_ptr.get(), ptr.get());
    EXPECT_EQ(weak_ptr->persistent().IsWeak(), false);

    ptr.reset();
  }

  EXPECT_EQ(env->base_object_count(), 1);
  EXPECT_NE(weak_ptr.get(), nullptr);
  EXPECT_EQ(weak_ptr->persistent().IsWeak(), true);

  v8::V8::SetFlagsFromString("--expose-gc");
  isolate_->RequestGarbageCollectionForTesting(Isolate::kFullGarbageCollection);

  EXPECT_EQ(env->base_object_count(), 0);
  EXPECT_EQ(weak_ptr.get(), nullptr);
}

TEST_F(BaseObjectPtrTest, Moveable) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env_{handle_scope, argv};
  Environment* env = *env_;

  BaseObjectPtr<DummyBaseObject> ptr = DummyBaseObject::NewDetached(env);
  EXPECT_EQ(env->base_object_count(), 1);
  BaseObjectWeakPtr<DummyBaseObject> weak_ptr { ptr };
  EXPECT_EQ(weak_ptr.get(), ptr.get());

  BaseObjectPtr<DummyBaseObject> ptr2 = std::move(ptr);
  EXPECT_EQ(weak_ptr.get(), ptr2.get());
  EXPECT_EQ(ptr.get(), nullptr);

  BaseObjectWeakPtr<DummyBaseObject> weak_ptr2 = std::move(weak_ptr);
  EXPECT_EQ(weak_ptr2.get(), ptr2.get());
  EXPECT_EQ(weak_ptr.get(), nullptr);
  EXPECT_EQ(env->base_object_count(), 1);

  ptr2.reset();

  EXPECT_EQ(weak_ptr2.get(), nullptr);
  EXPECT_EQ(env->base_object_count(), 0);
}

TEST_F(BaseObjectPtrTest, NestedClasses) {
  class ObjectWithPtr : public BaseObject {
   public:
    ObjectWithPtr(Environment* env, Local<Object> obj) : BaseObject(env, obj) {}

    BaseObjectPtr<BaseObject> ptr1;
    BaseObjectPtr<BaseObject> ptr2;

    SET_NO_MEMORY_INFO()
    SET_MEMORY_INFO_NAME(ObjectWithPtr)
    SET_SELF_SIZE(ObjectWithPtr)
  };

  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env_{handle_scope, argv};
  Environment* env = *env_;

  node::AddEnvironmentCleanupHook(isolate_, [](void* arg) {
    EXPECT_EQ(static_cast<Environment*>(arg)->base_object_count(), 0);
  }, env);

  ObjectWithPtr* obj =
      new ObjectWithPtr(env, DummyBaseObject::MakeJSObject(env));
  obj->ptr1 = DummyBaseObject::NewDetached(env);
  obj->ptr2 = DummyBaseObject::New(env);

  EXPECT_EQ(env->base_object_count(), 3);
}
