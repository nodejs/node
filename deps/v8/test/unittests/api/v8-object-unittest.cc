// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-function.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-template.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

using ObjectTest = TestWithContext;

void accessor_name_getter_callback(Local<Name>,
                                   const PropertyCallbackInfo<Value>&) {}

TEST_F(ObjectTest, SetAccessorWhenUnconfigurablePropAlreadyDefined) {
  TryCatch try_catch(isolate());

  Local<Object> global = context()->Global();
  Local<String> property_name = String::NewFromUtf8Literal(isolate(), "foo");

  PropertyDescriptor prop_desc;
  prop_desc.set_configurable(false);
  global->DefineProperty(context(), property_name, prop_desc).ToChecked();

  Maybe<bool> result = global->SetNativeDataProperty(
      context(), property_name, accessor_name_getter_callback);
  ASSERT_TRUE(result.IsJust());
  ASSERT_FALSE(result.FromJust());
  ASSERT_FALSE(try_catch.HasCaught());
}

using LapContextTest = TestWithIsolate;
// This tag value has been picked arbitrarily between 0 and
// V_EXTERNAL_POINTER_TAG_COUNT.
constexpr v8::ExternalPointerTypeTag kContextTag = 25;

TEST_F(LapContextTest, CurrentContextInLazyAccessorOnPrototype) {
  // The receiver object is created in |receiver_context|, but its prototype
  // object is created in |prototype_context|, and the property is accessed
  // from |caller_context|.
  Local<Context> receiver_context = Context::New(isolate());
  Local<Context> prototype_context = Context::New(isolate());
  Local<Context> caller_context = Context::New(isolate());

  static int call_count;  // The number of calls of the accessor callback.
  call_count = 0;

  Local<FunctionTemplate> function_template = FunctionTemplate::New(isolate());
  Local<Signature> signature = Signature::New(isolate(), function_template);
  Local<String> property_key =
      String::NewFromUtf8Literal(isolate(), "property");
  Local<FunctionTemplate> get_or_set = FunctionTemplate::New(
      isolate(),
      [](const FunctionCallbackInfo<Value>& info) {
        ++call_count;
        Local<Context> prototype_context = *reinterpret_cast<Local<Context>*>(
            info.DataV2().As<External>()->Value(kContextTag));
        EXPECT_EQ(prototype_context, info.GetIsolate()->GetCurrentContext());
      },
      External::New(isolate(), &prototype_context, kContextTag), signature);
  function_template->PrototypeTemplate()->SetAccessorProperty(
      property_key, get_or_set, get_or_set);

  // |object| is created in |receiver_context|, and |prototype| is created
  // in |prototype_context|.  And then, object.__proto__ = prototype.
  Local<Function> interface_for_receiver =
      function_template->GetFunction(receiver_context).ToLocalChecked();
  Local<Function> interface_for_prototype =
      function_template->GetFunction(prototype_context).ToLocalChecked();
  Local<String> prototype_key =
      String::NewFromUtf8Literal(isolate(), "prototype");
  Local<Object> prototype =
      interface_for_prototype->Get(caller_context, prototype_key)
          .ToLocalChecked()
          .As<Object>();
  Local<Object> object =
      interface_for_receiver->NewInstance(receiver_context).ToLocalChecked();
  object->SetPrototype(caller_context, prototype).ToChecked();
  EXPECT_EQ(receiver_context,
            object->GetCreationContext(isolate()).ToLocalChecked());
  EXPECT_EQ(prototype_context,
            prototype->GetCreationContext(isolate()).ToLocalChecked());

  EXPECT_EQ(0, call_count);
  object->Get(caller_context, property_key).ToLocalChecked();
  EXPECT_EQ(1, call_count);
  object->Set(caller_context, property_key, Null(isolate())).ToChecked();
  EXPECT_EQ(2, call_count);

  // Test with a compiled version.
  Local<String> object_key = String::NewFromUtf8Literal(isolate(), "object");
  caller_context->Global()->Set(caller_context, object_key, object).ToChecked();
  const char script[] =
      "function f() { object.property; object.property = 0; } "
      "%PrepareFunctionForOptimization(f); "
      "f(); f(); "
      "%OptimizeFunctionOnNextCall(f); "
      "f();";
  Context::Scope scope(caller_context);
  internal::v8_flags.allow_natives_syntax.Set(true);
  Script::Compile(caller_context, String::NewFromUtf8Literal(isolate(), script))
      .ToLocalChecked()
      ->Run(caller_context)
      .ToLocalChecked();
  EXPECT_EQ(8, call_count);
}

TEST_F(LapContextTest, CurrentContextInLazyAccessorOnPlatformObject) {
  Local<Context> receiver_context = Context::New(isolate());
  Local<Context> caller_context = Context::New(isolate());

  static int call_count;  // The number of calls of the accessor callback.
  call_count = 0;

  Local<FunctionTemplate> function_template = FunctionTemplate::New(isolate());
  Local<Signature> signature = Signature::New(isolate(), function_template);
  Local<String> property_key =
      String::NewFromUtf8Literal(isolate(), "property");
  Local<FunctionTemplate> get_or_set = FunctionTemplate::New(
      isolate(),
      [](const FunctionCallbackInfo<Value>& info) {
        ++call_count;
        Local<Context> receiver_context = *reinterpret_cast<Local<Context>*>(
            info.DataV2().As<External>()->Value(kContextTag));
        EXPECT_EQ(receiver_context, info.GetIsolate()->GetCurrentContext());
      },
      External::New(isolate(), &receiver_context, kContextTag), signature);
  function_template->InstanceTemplate()->SetAccessorProperty(
      property_key, get_or_set, get_or_set);

  Local<Function> interface =
      function_template->GetFunction(receiver_context).ToLocalChecked();
  Local<Object> object =
      interface->NewInstance(receiver_context).ToLocalChecked();

  EXPECT_EQ(0, call_count);
  object->Get(caller_context, property_key).ToLocalChecked();
  EXPECT_EQ(1, call_count);
  object->Set(caller_context, property_key, Null(isolate())).ToChecked();
  EXPECT_EQ(2, call_count);

  // Test with a compiled version.
  Local<String> object_key = String::NewFromUtf8Literal(isolate(), "object");
  caller_context->Global()->Set(caller_context, object_key, object).ToChecked();
  const char script[] =
      "function f() { object.property; object.property = 0; } "
      "%PrepareFunctionForOptimization(f);"
      "f(); f(); "
      "%OptimizeFunctionOnNextCall(f); "
      "f();";
  Context::Scope scope(caller_context);
  internal::v8_flags.allow_natives_syntax.Set(true);
  Script::Compile(caller_context, String::NewFromUtf8Literal(isolate(), script))
      .ToLocalChecked()
      ->Run(caller_context)
      .ToLocalChecked();
  EXPECT_EQ(8, call_count);
}

TEST_F(LapContextTest, CurrentContextInLazyAccessorOnInterface) {
  Local<Context> interface_context = Context::New(isolate());
  Local<Context> caller_context = Context::New(isolate());

  static int call_count;  // The number of calls of the accessor callback.
  call_count = 0;

  Local<FunctionTemplate> function_template = FunctionTemplate::New(isolate());
  Local<String> property_key =
      String::NewFromUtf8Literal(isolate(), "property");
  Local<FunctionTemplate> get_or_set = FunctionTemplate::New(
      isolate(),
      [](const FunctionCallbackInfo<Value>& info) {
        ++call_count;
        Local<Context> interface_context = *reinterpret_cast<Local<Context>*>(
            info.DataV2().As<External>()->Value(kContextTag));
        EXPECT_EQ(interface_context, info.GetIsolate()->GetCurrentContext());
      },
      External::New(isolate(), &interface_context, kContextTag),
      Local<Signature>());
  function_template->SetAccessorProperty(property_key, get_or_set, get_or_set);

  Local<Function> interface =
      function_template->GetFunction(interface_context).ToLocalChecked();

  EXPECT_EQ(0, call_count);
  interface->Get(caller_context, property_key).ToLocalChecked();
  EXPECT_EQ(1, call_count);
  interface->Set(caller_context, property_key, Null(isolate())).ToChecked();
  EXPECT_EQ(2, call_count);

  // Test with a compiled version.
  Local<String> interface_key =
      String::NewFromUtf8Literal(isolate(), "Interface");
  caller_context->Global()
      ->Set(caller_context, interface_key, interface)
      .ToChecked();
  const char script[] =
      "function f() { Interface.property; Interface.property = 0; } "
      "%PrepareFunctionForOptimization(f);"
      "f(); f(); "
      "%OptimizeFunctionOnNextCall(f); "
      "f();";
  Context::Scope scope(caller_context);
  internal::v8_flags.allow_natives_syntax.Set(true);
  Script::Compile(caller_context, String::NewFromUtf8Literal(isolate(), script))
      .ToLocalChecked()
      ->Run(caller_context)
      .ToLocalChecked();
  EXPECT_EQ(8, call_count);
}

class TryGetWithNoSideEffectsTest : public TestWithContext {
 protected:
  // Calls TryGetWithNoSideEffects(), which must neither run script nor throw.
  MaybeLocal<Value> TryGet(Local<Object> object, Local<Name> key,
                           Local<Context> context) {
    TryCatch try_catch(isolate());
    Isolate::DisallowJavascriptExecutionScope no_script(
        isolate(), Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
    MaybeLocal<Value> result = object->TryGetWithNoSideEffects(context, key);
    EXPECT_FALSE(try_catch.HasCaught());
    return result;
  }

  MaybeLocal<Value> TryGet(Local<Object> object, const char* key) {
    return TryGet(object, NewString(key), context());
  }

  // Expects TryGetWithNoSideEffects() to return the value of the JavaScript
  // expression `expected`.
  void ExpectValue(Local<Object> object, const char* key,
                   const char* expected) {
    Local<Value> value;
    ASSERT_TRUE(TryGet(object, key).ToLocal(&value)) << key;
    EXPECT_TRUE(value->StrictEquals(RunJS(expected))) << key;
  }

  void ExpectEmpty(Local<Object> object, const char* key) {
    EXPECT_TRUE(TryGet(object, key).IsEmpty()) << key;
  }

  Local<Object> RunJSObject(const char* source) {
    return RunJS(source).As<Object>();
  }

  int32_t RunJSInt32(const char* source) {
    return RunJS(source)->Int32Value(context()).FromJust();
  }
};

TEST_F(TryGetWithNoSideEffectsTest, DataProperties) {
  Local<Object> object = RunJSObject(R"(
      var proto = {inherited: 'inherited', shadowed: 'proto'};
      var object = Object.create(proto);
      object.own = 'own';
      object.shadowed = 'own';
      object.undefined_value = undefined;
      object[0] = 'element';
      object[Symbol.for('symbol')] = 'symbol';
      Object.defineProperty(object, 'non_configurable', {value: 'nc'});
      object;
  )");
  ExpectValue(object, "own", "'own'");
  ExpectValue(object, "inherited", "'inherited'");
  ExpectValue(object, "shadowed", "'own'");
  ExpectValue(object, "undefined_value", "undefined");
  ExpectValue(object, "0", "'element'");
  ExpectValue(object, "non_configurable", "'nc'");
  ExpectValue(object, "toString", "Object.prototype.toString");
  Local<Value> value;
  ASSERT_TRUE(
      TryGet(object, Symbol::For(isolate(), NewString("symbol")), context())
          .ToLocal(&value));
  EXPECT_TRUE(value->StrictEquals(NewString("symbol")));

  ExpectValue(RunJSObject("Object.freeze({frozen: 'frozen'})"), "frozen",
              "'frozen'");
  ExpectValue(RunJSObject("[1, 2]"), "1", "2");
  ExpectValue(RunJSObject("new Uint8Array([1, 2])"), "1", "2");
}

TEST_F(TryGetWithNoSideEffectsTest, GlobalObject) {
  RunJS("var global_var = 'var'; globalThis.global_property = 'property';");
  Local<Object> global = context()->Global();
  ExpectValue(global, "global_var", "'var'");
  ExpectValue(global, "global_property", "'property'");
  ExpectValue(global, "Object", "Object");
  ExpectEmpty(global, "missing");
}

TEST_F(TryGetWithNoSideEffectsTest, MissingProperties) {
  ExpectEmpty(RunJSObject("({})"), "missing");
  ExpectEmpty(RunJSObject("[]"), "0");
  ExpectEmpty(RunJSObject("Object.create(null)"), "toString");
  // Out-of-bounds typed array elements are not looked up on the prototype
  // chain.
  ExpectEmpty(RunJSObject("Uint8Array.prototype[2] = 'proto';"
                          "new Uint8Array(2)"),
              "2");
}

TEST_F(TryGetWithNoSideEffectsTest, AccessorsAreNotCalled) {
  Local<Object> object = RunJSObject(R"(
      var calls = 0;
      function getter() { calls++; return 'getter'; }
      function setter(value) { calls++; }
      var proto = Object.defineProperty({}, 'inherited', {get: getter});
      var object = Object.create(proto);
      Object.defineProperty(object, 'own', {get: getter, configurable: true});
      Object.defineProperty(object, 'non_configurable', {get: getter});
      Object.defineProperty(object, 'setter_only', {set: setter});
      object;
  )");
  ExpectEmpty(object, "own");
  ExpectEmpty(object, "inherited");
  ExpectEmpty(object, "non_configurable");
  ExpectEmpty(object, "setter_only");

  // Data properties shadowing accessors can be read.
  Local<Object> shadowing = RunJSObject(
      "Object.defineProperty(Object.create(object), 'own', {value: 'data'})");
  ExpectValue(shadowing, "own", "'data'");
  ExpectEmpty(shadowing, "inherited");
  EXPECT_EQ(0, RunJSInt32("calls"));
}

// Getters are never called, not even native functions declared side-effect
// free, as script can install any function as a getter on any object.
TEST_F(TryGetWithNoSideEffectsTest, NativeGettersAreNotCalled) {
  static int calls;
  calls = 0;
  Local<FunctionTemplate> getter = FunctionTemplate::New(
      isolate(),
      [](const FunctionCallbackInfo<Value>& info) {
        ++calls;
        info.GetReturnValue().Set(42);
      },
      Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow,
      SideEffectType::kHasNoSideEffect);
  Local<ObjectTemplate> object_template = ObjectTemplate::New(isolate());
  object_template->SetAccessorProperty(NewString("getter"), getter);
  Local<Object> object =
      object_template->NewInstance(context()).ToLocalChecked();
  SetGlobalProperty("object", object);
  ExpectEmpty(object, "getter");

  Local<Object> array = RunJSObject(R"(
      var array = [1, 2, 3];
      Object.defineProperty(array, 'getter',
                            Object.getOwnPropertyDescriptor(object, 'getter'));
      Object.defineProperty(array, 'pop', {get: Array.prototype.pop});
      array;
  )");
  ExpectEmpty(array, "getter");
  ExpectEmpty(array, "pop");
  EXPECT_EQ(0, calls);
  ExpectValue(array, "length", "3");

  // Unlike TryGetWithNoSideEffects(), Get() calls the getter.
  EXPECT_FALSE(object->Get(context(), NewString("getter")).IsEmpty());
  EXPECT_EQ(1, calls);
}

// Properties like these are implemented as side-effect free native data
// properties, which behave like data properties.
TEST_F(TryGetWithNoSideEffectsTest, BuiltinNativeDataProperties) {
  Local<Object> function = RunJSObject("function f(a, b) {}; f");
  ExpectValue(function, "prototype", "f.prototype");
  ExpectValue(function, "name", "'f'");
  ExpectValue(function, "length", "2");
  ExpectValue(RunJSObject("[1, 2, 3]"), "length", "3");
  ExpectValue(RunJSObject("new String('abc')"), "length", "3");
}

TEST_F(TryGetWithNoSideEffectsTest, EmbedderNativeDataProperties) {
  static int calls;
  calls = 0;
  AccessorNameGetterCallback getter =
      [](Local<Name>, const PropertyCallbackInfo<Value>& info) {
        ++calls;
        info.GetReturnValue().Set(42);
      };
  Local<Object> object = Object::New(isolate());
  ASSERT_TRUE(
      object->SetNativeDataProperty(context(), NewString("native"), getter)
          .FromJust());
  ASSERT_TRUE(object->SetLazyDataProperty(context(), NewString("lazy"), getter)
                  .FromJust());
  ASSERT_TRUE(object
                  ->SetNativeDataProperty(context(),
                                          NewString("side_effect_free_native"),
                                          getter, nullptr, Local<Value>(), None,
                                          SideEffectType::kHasNoSideEffect)
                  .FromJust());
  ASSERT_TRUE(object
                  ->SetLazyDataProperty(
                      context(), NewString("side_effect_free_lazy"), getter,
                      Local<Value>(), None, SideEffectType::kHasNoSideEffect)
                  .FromJust());
  SetGlobalProperty("object", object);

  // Getters that may have side effects are not called.
  ExpectEmpty(object, "native");
  ExpectEmpty(object, "lazy");
  EXPECT_EQ(0, calls);

  ExpectValue(object, "side_effect_free_native", "42");
  EXPECT_EQ(1, calls);
  ExpectValue(RunJSObject("Object.create(object)"), "side_effect_free_native",
              "42");
  EXPECT_EQ(2, calls);
  ExpectValue(object, "side_effect_free_lazy", "42");
  EXPECT_EQ(3, calls);
  // The lazy data property has been replaced by a data property.
  ExpectValue(object, "side_effect_free_lazy", "42");
  EXPECT_EQ(3, calls);
}

TEST_F(TryGetWithNoSideEffectsTest, ThrowingNativeDataProperty) {
  AccessorNameGetterCallback getter =
      [](Local<Name>, const PropertyCallbackInfo<Value>& info) {
        info.GetIsolate()->ThrowError("error");
      };
  Local<Object> object = Object::New(isolate());
  ASSERT_TRUE(object
                  ->SetNativeDataProperty(context(), NewString("throws"),
                                          getter, nullptr, Local<Value>(), None,
                                          SideEffectType::kHasNoSideEffect)
                  .FromJust());
  // The exception doesn't escape, which TryGet() checks.
  ExpectEmpty(object, "throws");
}

// Embedders may install interface objects (e.g. `Node` in Blink) as lazy data
// properties on the global object. Their `prototype` holds the methods.
TEST_F(TryGetWithNoSideEffectsTest, InterfaceObjectOnGlobalObject) {
  static int calls;
  calls = 0;
  AccessorNameGetterCallback getter =
      [](Local<Name>, const PropertyCallbackInfo<Value>& info) {
        ++calls;
        Isolate* isolate = info.GetIsolate();
        Local<FunctionTemplate> interface_template =
            FunctionTemplate::New(isolate);
        interface_template->ReadOnlyPrototype();
        interface_template->PrototypeTemplate()->Set(
            isolate, "method", FunctionTemplate::New(isolate));
        info.GetReturnValue().Set(
            interface_template->GetFunction(isolate->GetCurrentContext())
                .ToLocalChecked());
      };
  Local<Object> global = context()->Global();
  ASSERT_TRUE(global
                  ->SetLazyDataProperty(context(), NewString("Interface"),
                                        getter, Local<Value>(), DontEnum,
                                        SideEffectType::kHasNoSideEffect)
                  .FromJust());

  Local<Value> interface;
  ASSERT_TRUE(TryGet(global, "Interface").ToLocal(&interface));
  ASSERT_TRUE(interface->IsFunction());
  Local<Value> prototype;
  ASSERT_TRUE(TryGet(interface.As<Object>(), "prototype").ToLocal(&prototype));
  ASSERT_TRUE(prototype->IsObject());
  Local<Value> method;
  ASSERT_TRUE(TryGet(prototype.As<Object>(), "method").ToLocal(&method));
  EXPECT_TRUE(method->StrictEquals(RunJS("Interface.prototype.method")));
  EXPECT_EQ(1, calls);
}

TEST_F(TryGetWithNoSideEffectsTest, InterceptorsAreNotCalled) {
  static int calls;
  Local<ObjectTemplate> object_template = ObjectTemplate::New(isolate());
  object_template->SetHandler(NamedPropertyHandlerConfiguration(
      [](Local<Name>, const PropertyCallbackInfo<Value>& info) {
        ++calls;
        info.GetReturnValue().Set(42);
        return Intercepted::kYes;
      }));
  object_template->SetHandler(IndexedPropertyHandlerConfiguration(
      [](uint32_t, const PropertyCallbackInfo<Value>& info) {
        ++calls;
        info.GetReturnValue().Set(42);
        return Intercepted::kYes;
      }));
  object_template->Set(isolate(), "own", NewString("own"));
  Local<Object> object =
      object_template->NewInstance(context()).ToLocalChecked();
  SetGlobalProperty("object", object);
  Local<Object> derived = RunJSObject(R"(
      var derived = Object.create(object);
      Object.defineProperty(derived, 'own', {value: 'derived'});
      derived;
  )");

  calls = 0;
  // Interceptors are consulted before the object's own properties.
  ExpectEmpty(object, "own");
  ExpectEmpty(object, "missing");
  ExpectEmpty(object, "0");
  // Properties found before reaching the interceptor can be read.
  ExpectValue(derived, "own", "'derived'");
  ExpectEmpty(derived, "missing");
  EXPECT_EQ(0, calls);
}

TEST_F(TryGetWithNoSideEffectsTest, NonMaskingInterceptorsAreNotCalled) {
  static int calls;
  Local<ObjectTemplate> object_template = ObjectTemplate::New(isolate());
  object_template->SetHandler(NamedPropertyHandlerConfiguration(
      [](Local<Name>, const PropertyCallbackInfo<Value>& info) {
        ++calls;
        info.GetReturnValue().Set(42);
        return Intercepted::kYes;
      },
      nullptr, nullptr, nullptr, nullptr, Local<Value>(),
      PropertyHandlerFlags::kNonMasking));
  object_template->Set(isolate(), "own", NewString("own"));
  Local<Object> object =
      object_template->NewInstance(context()).ToLocalChecked();

  calls = 0;
  // Non-masking interceptors are only consulted for properties that are not
  // found on the prototype chain.
  ExpectValue(object, "own", "'own'");
  ExpectValue(object, "toString", "Object.prototype.toString");
  ExpectEmpty(object, "missing");
  EXPECT_EQ(0, calls);
}

TEST_F(TryGetWithNoSideEffectsTest, ProxiesAreNotCalled) {
  Local<Object> proxy = RunJSObject(R"(
      var trap_lookups = 0;
      var handler = new Proxy({}, {
        get() {
          trap_lookups++;
          return undefined;
        }
      });
      var proxy = new Proxy({data: 'data'}, handler);
      var derived = Object.defineProperty(Object.create(proxy), 'own',
                                          {value: 'own'});
      proxy;
  )");
  ExpectEmpty(proxy, "data");
  ExpectEmpty(proxy, "missing");
  // Properties found before reaching the proxy can be read.
  Local<Object> derived = RunJSObject("derived");
  ExpectValue(derived, "own", "'own'");
  ExpectEmpty(derived, "data");
  EXPECT_EQ(0, RunJSInt32("trap_lookups"));

  // Proxies are not looked through, even if they have no traps.
  ExpectEmpty(RunJSObject("new Proxy({data: 'data'}, {})"), "data");
  // Unlike Get(), this doesn't throw for revoked proxies.
  ExpectEmpty(RunJSObject(R"(
      var {proxy: revoked, revoke} = Proxy.revocable({data: 'data'}, {});
      revoke();
      revoked;
  )"),
              "data");
}

TEST_F(TryGetWithNoSideEffectsTest, AccessChecks) {
  static bool allow_access;
  static int access_checks;
  static int failed_access_checks;
  isolate()->SetFailedAccessCheckCallbackFunction(
      [](Local<Object>, AccessType, Local<Value>) { ++failed_access_checks; });
  Local<ObjectTemplate> object_template = ObjectTemplate::New(isolate());
  object_template->SetAccessCheckCallback(
      [](Local<Context>, Local<Object>, Local<Value>) {
        ++access_checks;
        return allow_access;
      });
  object_template->Set(isolate(), "data", NewString("data"));
  allow_access = true;
  Local<Object> object =
      object_template->NewInstance(context()).ToLocalChecked();
  failed_access_checks = 0;

  access_checks = 0;
  ExpectValue(object, "data", "'data'");
  EXPECT_LT(0, access_checks);

  access_checks = 0;
  allow_access = false;
  ExpectEmpty(object, "data");
  EXPECT_LT(0, access_checks);
  EXPECT_EQ(0, failed_access_checks);
}

TEST_F(TryGetWithNoSideEffectsTest, CrossContextAccess) {
  RunJS("var data = 'data';");
  Local<Object> global = context()->Global();
  Local<Name> key = NewString("data");
  Local<Context> other_context = Context::New(isolate());
  EXPECT_TRUE(TryGet(global, key, other_context).IsEmpty());

  Local<Value> token = NewString("token");
  context()->SetSecurityToken(token);
  other_context->SetSecurityToken(token);
  Local<Value> value;
  ASSERT_TRUE(TryGet(global, key, other_context).ToLocal(&value));
  EXPECT_TRUE(value->StrictEquals(NewString("data")));
}

}  // namespace
}  // namespace v8
