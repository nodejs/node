// Copyright 2006-2008 the V8 project authors. All rights reserved.

#include <stdlib.h>

#include "v8.h"

#include "execution.h"
#include "factory.h"
#include "macro-assembler.h"
#include "global-handles.h"
#include "cctest.h"

using namespace v8::internal;

static v8::Persistent<v8::Context> env;

static void InitializeVM() {
  if (env.IsEmpty()) env = v8::Context::New();
  v8::HandleScope scope;
  env->Enter();
}


static void CheckMap(Map* map, int type, int instance_size) {
  CHECK(map->IsHeapObject());
#ifdef DEBUG
  CHECK(Heap::Contains(map));
#endif
  CHECK_EQ(Heap::meta_map(), map->map());
  CHECK_EQ(type, map->instance_type());
  CHECK_EQ(instance_size, map->instance_size());
}


TEST(HeapMaps) {
  InitializeVM();
  CheckMap(Heap::meta_map(), MAP_TYPE, Map::kSize);
  CheckMap(Heap::heap_number_map(), HEAP_NUMBER_TYPE, HeapNumber::kSize);
  CheckMap(Heap::fixed_array_map(), FIXED_ARRAY_TYPE, Array::kAlignedSize);
  CheckMap(Heap::long_string_map(), LONG_STRING_TYPE,
           SeqTwoByteString::kAlignedSize);
}


static void CheckOddball(Object* obj, const char* string) {
  CHECK(obj->IsOddball());
#ifndef V8_HOST_ARCH_64_BIT
// TODO(X64): Reenable when native builtins work.
  bool exc;
  Object* print_string = *Execution::ToString(Handle<Object>(obj), &exc);
  CHECK(String::cast(print_string)->IsEqualTo(CStrVector(string)));
#endif  // V8_HOST_ARCH_64_BIT
}


static void CheckSmi(int value, const char* string) {
#ifndef V8_HOST_ARCH_64_BIT
// TODO(X64): Reenable when native builtins work.
  bool exc;
  Object* print_string =
      *Execution::ToString(Handle<Object>(Smi::FromInt(value)), &exc);
  CHECK(String::cast(print_string)->IsEqualTo(CStrVector(string)));
#endif  // V8_HOST_ARCH_64_BIT
}


static void CheckNumber(double value, const char* string) {
  Object* obj = Heap::NumberFromDouble(value);
  CHECK(obj->IsNumber());
#ifndef V8_HOST_ARCH_64_BIT
// TODO(X64): Reenable when native builtins work.
  bool exc;
  Object* print_string = *Execution::ToString(Handle<Object>(obj), &exc);
  CHECK(String::cast(print_string)->IsEqualTo(CStrVector(string)));
#endif  // V8_HOST_ARCH_64_BIT
}


static void CheckFindCodeObject() {
  // Test FindCodeObject
#define __ assm.

  Assembler assm(NULL, 0);

  __ nop();  // supported on all architectures

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());

  HeapObject* obj = HeapObject::cast(code);
  Address obj_addr = obj->address();

  for (int i = 0; i < obj->Size(); i += kPointerSize) {
    Object* found = Heap::FindCodeObject(obj_addr + i);
    CHECK_EQ(code, found);
  }

  Object* copy = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(copy->IsCode());
  HeapObject* obj_copy = HeapObject::cast(copy);
  Object* not_right = Heap::FindCodeObject(obj_copy->address() +
                                           obj_copy->Size() / 2);
  CHECK(not_right != code);
}


TEST(HeapObjects) {
  InitializeVM();

  v8::HandleScope sc;
  Object* value = Heap::NumberFromDouble(1.000123);
  CHECK(value->IsHeapNumber());
  CHECK(value->IsNumber());
  CHECK_EQ(1.000123, value->Number());

  value = Heap::NumberFromDouble(1.0);
  CHECK(value->IsSmi());
  CHECK(value->IsNumber());
  CHECK_EQ(1.0, value->Number());

  value = Heap::NumberFromInt32(1024);
  CHECK(value->IsSmi());
  CHECK(value->IsNumber());
  CHECK_EQ(1024.0, value->Number());

  value = Heap::NumberFromInt32(Smi::kMinValue);
  CHECK(value->IsSmi());
  CHECK(value->IsNumber());
  CHECK_EQ(Smi::kMinValue, Smi::cast(value)->value());

  value = Heap::NumberFromInt32(Smi::kMaxValue);
  CHECK(value->IsSmi());
  CHECK(value->IsNumber());
  CHECK_EQ(Smi::kMaxValue, Smi::cast(value)->value());

  value = Heap::NumberFromInt32(Smi::kMinValue - 1);
  CHECK(value->IsHeapNumber());
  CHECK(value->IsNumber());
  CHECK_EQ(static_cast<double>(Smi::kMinValue - 1), value->Number());

  value = Heap::NumberFromInt32(Smi::kMaxValue + 1);
  CHECK(value->IsHeapNumber());
  CHECK(value->IsNumber());
  CHECK_EQ(static_cast<double>(Smi::kMaxValue + 1), value->Number());

  // nan oddball checks
  CHECK(Heap::nan_value()->IsNumber());
  CHECK(isnan(Heap::nan_value()->Number()));

  Object* str = Heap::AllocateStringFromAscii(CStrVector("fisk hest "));
  if (!str->IsFailure()) {
    String* s =  String::cast(str);
    CHECK(s->IsString());
    CHECK_EQ(10, s->length());
  } else {
    CHECK(false);
  }

  String* object_symbol = String::cast(Heap::Object_symbol());
  CHECK(Top::context()->global()->HasLocalProperty(object_symbol));

  // Check ToString for oddballs
  CheckOddball(Heap::true_value(), "true");
  CheckOddball(Heap::false_value(), "false");
  CheckOddball(Heap::null_value(), "null");
  CheckOddball(Heap::undefined_value(), "undefined");

  // Check ToString for Smis
  CheckSmi(0, "0");
  CheckSmi(42, "42");
  CheckSmi(-42, "-42");

  // Check ToString for Numbers
  CheckNumber(1.1, "1.1");

  CheckFindCodeObject();
}


TEST(Tagging) {
  InitializeVM();
  int request = 24;
  ASSERT_EQ(request, OBJECT_SIZE_ALIGN(request));
  CHECK(Smi::FromInt(42)->IsSmi());
  CHECK(Failure::RetryAfterGC(request, NEW_SPACE)->IsFailure());
  CHECK_EQ(request, Failure::RetryAfterGC(request, NEW_SPACE)->requested());
  CHECK_EQ(NEW_SPACE,
           Failure::RetryAfterGC(request, NEW_SPACE)->allocation_space());
  CHECK_EQ(OLD_POINTER_SPACE,
           Failure::RetryAfterGC(request,
                                 OLD_POINTER_SPACE)->allocation_space());
  CHECK(Failure::Exception()->IsFailure());
  CHECK(Smi::FromInt(Smi::kMinValue)->IsSmi());
  CHECK(Smi::FromInt(Smi::kMaxValue)->IsSmi());
}


TEST(GarbageCollection) {
  InitializeVM();

  v8::HandleScope sc;
  // check GC when heap is empty
  int free_bytes = Heap::MaxObjectSizeInPagedSpace();
  CHECK(Heap::CollectGarbage(free_bytes, NEW_SPACE));

  // allocate a function and keep it in global object's property
  String* func_name  = String::cast(Heap::LookupAsciiSymbol("theFunction"));
  SharedFunctionInfo* function_share =
    SharedFunctionInfo::cast(Heap::AllocateSharedFunctionInfo(func_name));
  JSFunction* function =
      JSFunction::cast(Heap::AllocateFunction(*Top::function_map(),
                                              function_share,
                                              Heap::undefined_value()));
  Map* initial_map =
      Map::cast(Heap::AllocateMap(JS_OBJECT_TYPE, JSObject::kHeaderSize));
  function->set_initial_map(initial_map);
  Top::context()->global()->SetProperty(func_name, function, NONE);

  // allocate an object, but it is unrooted
  String* prop_name = String::cast(Heap::LookupAsciiSymbol("theSlot"));
  String* prop_namex = String::cast(Heap::LookupAsciiSymbol("theSlotx"));
  JSObject* obj = JSObject::cast(Heap::AllocateJSObject(function));
  obj->SetProperty(prop_name, Smi::FromInt(23), NONE);
  obj->SetProperty(prop_namex, Smi::FromInt(24), NONE);

  CHECK_EQ(Smi::FromInt(23), obj->GetProperty(prop_name));
  CHECK_EQ(Smi::FromInt(24), obj->GetProperty(prop_namex));

  CHECK(Heap::CollectGarbage(free_bytes, NEW_SPACE));

  // function should be alive, func_name might be invalid after GC
  func_name  = String::cast(Heap::LookupAsciiSymbol("theFunction"));
  CHECK(Top::context()->global()->HasLocalProperty(func_name));
  // check function is retained
  Object* func_value = Top::context()->global()->GetProperty(func_name);
  CHECK(func_value->IsJSFunction());
  // old function pointer may not be valid
  function = JSFunction::cast(func_value);

  // allocate another object, make it reachable from global
  obj = JSObject::cast(Heap::AllocateJSObject(function));
  String* obj_name = String::cast(Heap::LookupAsciiSymbol("theObject"));
  Top::context()->global()->SetProperty(obj_name, obj, NONE);
  // set property
  prop_name = String::cast(Heap::LookupAsciiSymbol("theSlot"));
  obj->SetProperty(prop_name, Smi::FromInt(23), NONE);

  // after gc, it should survive
  CHECK(Heap::CollectGarbage(free_bytes, NEW_SPACE));

  obj_name = String::cast(Heap::LookupAsciiSymbol("theObject"));
  CHECK(Top::context()->global()->HasLocalProperty(obj_name));
  CHECK(Top::context()->global()->GetProperty(obj_name)->IsJSObject());
  obj = JSObject::cast(Top::context()->global()->GetProperty(obj_name));
  prop_name = String::cast(Heap::LookupAsciiSymbol("theSlot"));
  CHECK_EQ(Smi::FromInt(23), obj->GetProperty(prop_name));
}


static void VerifyStringAllocation(const char* string) {
  String* s = String::cast(Heap::AllocateStringFromUtf8(CStrVector(string)));
  CHECK_EQ(static_cast<int>(strlen(string)), s->length());
  for (int index = 0; index < s->length(); index++) {
    CHECK_EQ(static_cast<uint16_t>(string[index]), s->Get(index));  }
}


TEST(String) {
  InitializeVM();

  VerifyStringAllocation("a");
  VerifyStringAllocation("ab");
  VerifyStringAllocation("abc");
  VerifyStringAllocation("abcd");
  VerifyStringAllocation("fiskerdrengen er paa havet");
}


TEST(LocalHandles) {
  InitializeVM();

  v8::HandleScope scope;
  const char* name = "Kasper the spunky";
  Handle<String> string = Factory::NewStringFromAscii(CStrVector(name));
  CHECK_EQ(static_cast<int>(strlen(name)), string->length());
}


TEST(GlobalHandles) {
  InitializeVM();

  Object* i = Heap::AllocateStringFromAscii(CStrVector("fisk"));
  Object* u = Heap::AllocateHeapNumber(1.12344);

  Handle<Object> h1 = GlobalHandles::Create(i);
  Handle<Object> h2 = GlobalHandles::Create(u);
  Handle<Object> h3 = GlobalHandles::Create(i);
  Handle<Object> h4 = GlobalHandles::Create(u);

  // after gc, it should survive
  CHECK(Heap::CollectGarbage(0, NEW_SPACE));

  CHECK((*h1)->IsString());
  CHECK((*h2)->IsHeapNumber());
  CHECK((*h3)->IsString());
  CHECK((*h4)->IsHeapNumber());

  CHECK_EQ(*h3, *h1);
  GlobalHandles::Destroy(h1.location());
  GlobalHandles::Destroy(h3.location());

  CHECK_EQ(*h4, *h2);
  GlobalHandles::Destroy(h2.location());
  GlobalHandles::Destroy(h4.location());
}


static bool WeakPointerCleared = false;

static void TestWeakGlobalHandleCallback(v8::Persistent<v8::Value> handle,
                                         void* id) {
  USE(handle);
  if (1234 == reinterpret_cast<intptr_t>(id)) WeakPointerCleared = true;
}


TEST(WeakGlobalHandlesScavenge) {
  InitializeVM();

  WeakPointerCleared = false;

  Object* i = Heap::AllocateStringFromAscii(CStrVector("fisk"));
  Object* u = Heap::AllocateHeapNumber(1.12344);

  Handle<Object> h1 = GlobalHandles::Create(i);
  Handle<Object> h2 = GlobalHandles::Create(u);

  GlobalHandles::MakeWeak(h2.location(),
                          reinterpret_cast<void*>(1234),
                          &TestWeakGlobalHandleCallback);

  // Scavenge treats weak pointers as normal roots.
  Heap::PerformScavenge();

  CHECK((*h1)->IsString());
  CHECK((*h2)->IsHeapNumber());

  CHECK(!WeakPointerCleared);
  CHECK(!GlobalHandles::IsNearDeath(h2.location()));
  CHECK(!GlobalHandles::IsNearDeath(h1.location()));

  GlobalHandles::Destroy(h1.location());
  GlobalHandles::Destroy(h2.location());
}


TEST(WeakGlobalHandlesMark) {
  InitializeVM();

  WeakPointerCleared = false;

  Object* i = Heap::AllocateStringFromAscii(CStrVector("fisk"));
  Object* u = Heap::AllocateHeapNumber(1.12344);

  Handle<Object> h1 = GlobalHandles::Create(i);
  Handle<Object> h2 = GlobalHandles::Create(u);

  CHECK(Heap::CollectGarbage(0, OLD_POINTER_SPACE));
  CHECK(Heap::CollectGarbage(0, NEW_SPACE));
  // Make sure the object is promoted.

  GlobalHandles::MakeWeak(h2.location(),
                          reinterpret_cast<void*>(1234),
                          &TestWeakGlobalHandleCallback);
  CHECK(!GlobalHandles::IsNearDeath(h1.location()));
  CHECK(!GlobalHandles::IsNearDeath(h2.location()));

  CHECK(Heap::CollectGarbage(0, OLD_POINTER_SPACE));

  CHECK((*h1)->IsString());

  CHECK(WeakPointerCleared);
  CHECK(!GlobalHandles::IsNearDeath(h1.location()));
  CHECK(GlobalHandles::IsNearDeath(h2.location()));

  GlobalHandles::Destroy(h1.location());
  GlobalHandles::Destroy(h2.location());
}

static void TestDeleteWeakGlobalHandleCallback(
    v8::Persistent<v8::Value> handle,
    void* id) {
  if (1234 == reinterpret_cast<intptr_t>(id)) WeakPointerCleared = true;
  handle.Dispose();
}

TEST(DeleteWeakGlobalHandle) {
  InitializeVM();

  WeakPointerCleared = false;

  Object* i = Heap::AllocateStringFromAscii(CStrVector("fisk"));
  Handle<Object> h = GlobalHandles::Create(i);

  GlobalHandles::MakeWeak(h.location(),
                          reinterpret_cast<void*>(1234),
                          &TestDeleteWeakGlobalHandleCallback);

  // Scanvenge does not recognize weak reference.
  Heap::PerformScavenge();

  CHECK(!WeakPointerCleared);

  // Mark-compact treats weak reference properly.
  CHECK(Heap::CollectGarbage(0, OLD_POINTER_SPACE));

  CHECK(WeakPointerCleared);
}

static const char* not_so_random_string_table[] = {
  "abstract",
  "boolean",
  "break",
  "byte",
  "case",
  "catch",
  "char",
  "class",
  "const",
  "continue",
  "debugger",
  "default",
  "delete",
  "do",
  "double",
  "else",
  "enum",
  "export",
  "extends",
  "false",
  "final",
  "finally",
  "float",
  "for",
  "function",
  "goto",
  "if",
  "implements",
  "import",
  "in",
  "instanceof",
  "int",
  "interface",
  "long",
  "native",
  "new",
  "null",
  "package",
  "private",
  "protected",
  "public",
  "return",
  "short",
  "static",
  "super",
  "switch",
  "synchronized",
  "this",
  "throw",
  "throws",
  "transient",
  "true",
  "try",
  "typeof",
  "var",
  "void",
  "volatile",
  "while",
  "with",
  0
};


static void CheckSymbols(const char** strings) {
  for (const char* string = *strings; *strings != 0; string = *strings++) {
    Object* a = Heap::LookupAsciiSymbol(string);
    CHECK(a->IsSymbol());
    Object* b = Heap::LookupAsciiSymbol(string);
    CHECK_EQ(b, a);
    CHECK(String::cast(b)->IsEqualTo(CStrVector(string)));
  }
}


TEST(SymbolTable) {
  InitializeVM();

  CheckSymbols(not_so_random_string_table);
  CheckSymbols(not_so_random_string_table);
}


TEST(FunctionAllocation) {
  InitializeVM();

  v8::HandleScope sc;
  String* name  = String::cast(Heap::LookupAsciiSymbol("theFunction"));
  SharedFunctionInfo* function_share =
    SharedFunctionInfo::cast(Heap::AllocateSharedFunctionInfo(name));
  JSFunction* function =
    JSFunction::cast(Heap::AllocateFunction(*Top::function_map(),
                                            function_share,
                                            Heap::undefined_value()));
  Map* initial_map =
      Map::cast(Heap::AllocateMap(JS_OBJECT_TYPE, JSObject::kHeaderSize));
  function->set_initial_map(initial_map);

  String* prop_name = String::cast(Heap::LookupAsciiSymbol("theSlot"));
  JSObject* obj = JSObject::cast(Heap::AllocateJSObject(function));
  obj->SetProperty(prop_name, Smi::FromInt(23), NONE);
  CHECK_EQ(Smi::FromInt(23), obj->GetProperty(prop_name));
  // Check that we can add properties to function objects.
  function->SetProperty(prop_name, Smi::FromInt(24), NONE);
  CHECK_EQ(Smi::FromInt(24), function->GetProperty(prop_name));
}


TEST(ObjectProperties) {
  InitializeVM();

  v8::HandleScope sc;
  JSFunction* constructor =
      JSFunction::cast(
          Top::context()->global()->GetProperty(String::cast(
                                                    Heap::Object_symbol())));
  JSObject* obj = JSObject::cast(Heap::AllocateJSObject(constructor));
  String* first = String::cast(Heap::LookupAsciiSymbol("first"));
  String* second = String::cast(Heap::LookupAsciiSymbol("second"));

  // check for empty
  CHECK(!obj->HasLocalProperty(first));

  // add first
  obj->SetProperty(first, Smi::FromInt(1), NONE);
  CHECK(obj->HasLocalProperty(first));

  // delete first
  CHECK(obj->DeleteProperty(first, JSObject::NORMAL_DELETION));
  CHECK(!obj->HasLocalProperty(first));

  // add first and then second
  obj->SetProperty(first, Smi::FromInt(1), NONE);
  obj->SetProperty(second, Smi::FromInt(2), NONE);
  CHECK(obj->HasLocalProperty(first));
  CHECK(obj->HasLocalProperty(second));

  // delete first and then second
  CHECK(obj->DeleteProperty(first, JSObject::NORMAL_DELETION));
  CHECK(obj->HasLocalProperty(second));
  CHECK(obj->DeleteProperty(second, JSObject::NORMAL_DELETION));
  CHECK(!obj->HasLocalProperty(first));
  CHECK(!obj->HasLocalProperty(second));

  // add first and then second
  obj->SetProperty(first, Smi::FromInt(1), NONE);
  obj->SetProperty(second, Smi::FromInt(2), NONE);
  CHECK(obj->HasLocalProperty(first));
  CHECK(obj->HasLocalProperty(second));

  // delete second and then first
  CHECK(obj->DeleteProperty(second, JSObject::NORMAL_DELETION));
  CHECK(obj->HasLocalProperty(first));
  CHECK(obj->DeleteProperty(first, JSObject::NORMAL_DELETION));
  CHECK(!obj->HasLocalProperty(first));
  CHECK(!obj->HasLocalProperty(second));

  // check string and symbol match
  static const char* string1 = "fisk";
  String* s1 =
      String::cast(Heap::AllocateStringFromAscii(CStrVector(string1)));
  obj->SetProperty(s1, Smi::FromInt(1), NONE);
  CHECK(obj->HasLocalProperty(String::cast(Heap::LookupAsciiSymbol(string1))));

  // check symbol and string match
  static const char* string2 = "fugl";
  String* s2 = String::cast(Heap::LookupAsciiSymbol(string2));
  obj->SetProperty(s2, Smi::FromInt(1), NONE);
  CHECK(obj->HasLocalProperty(
            String::cast(Heap::AllocateStringFromAscii(CStrVector(string2)))));
}


TEST(JSObjectMaps) {
  InitializeVM();

  v8::HandleScope sc;
  String* name  = String::cast(Heap::LookupAsciiSymbol("theFunction"));
  SharedFunctionInfo* function_share =
    SharedFunctionInfo::cast(Heap::AllocateSharedFunctionInfo(name));
  JSFunction* function =
    JSFunction::cast(Heap::AllocateFunction(*Top::function_map(),
                                            function_share,
                                            Heap::undefined_value()));
  Map* initial_map =
      Map::cast(Heap::AllocateMap(JS_OBJECT_TYPE, JSObject::kHeaderSize));
  function->set_initial_map(initial_map);
  String* prop_name = String::cast(Heap::LookupAsciiSymbol("theSlot"));
  JSObject* obj = JSObject::cast(Heap::AllocateJSObject(function));

  // Set a propery
  obj->SetProperty(prop_name, Smi::FromInt(23), NONE);
  CHECK_EQ(Smi::FromInt(23), obj->GetProperty(prop_name));

  // Check the map has changed
  CHECK(initial_map != obj->map());
}


TEST(JSArray) {
  InitializeVM();

  v8::HandleScope sc;
  String* name = String::cast(Heap::LookupAsciiSymbol("Array"));
  JSFunction* function =
      JSFunction::cast(Top::context()->global()->GetProperty(name));

  // Allocate the object.
  JSArray* array = JSArray::cast(Heap::AllocateJSObject(function));
  array->Initialize(0);

  // Set array length to 0.
  array->SetElementsLength(Smi::FromInt(0));
  CHECK_EQ(Smi::FromInt(0), array->length());
  CHECK(array->HasFastElements());  // Must be in fast mode.

  // array[length] = name.
  array->SetElement(0, name);
  CHECK_EQ(Smi::FromInt(1), array->length());
  CHECK_EQ(array->GetElement(0), name);

  // Set array length with larger than smi value.
  Object* length = Heap::NumberFromInt32(Smi::kMaxValue + 1);
  array->SetElementsLength(length);

  uint32_t int_length = 0;
  CHECK(Array::IndexFromObject(length, &int_length));
  CHECK_EQ(length, array->length());
  CHECK(!array->HasFastElements());  // Must be in slow mode.

  // array[length] = name.
  array->SetElement(int_length, name);
  uint32_t new_int_length = 0;
  CHECK(Array::IndexFromObject(array->length(), &new_int_length));
  CHECK_EQ(static_cast<double>(int_length), new_int_length - 1);
  CHECK_EQ(array->GetElement(int_length), name);
  CHECK_EQ(array->GetElement(0), name);
}


TEST(JSObjectCopy) {
  InitializeVM();

  v8::HandleScope sc;
  String* name = String::cast(Heap::Object_symbol());
  JSFunction* constructor =
      JSFunction::cast(Top::context()->global()->GetProperty(name));
  JSObject* obj = JSObject::cast(Heap::AllocateJSObject(constructor));
  String* first = String::cast(Heap::LookupAsciiSymbol("first"));
  String* second = String::cast(Heap::LookupAsciiSymbol("second"));

  obj->SetProperty(first, Smi::FromInt(1), NONE);
  obj->SetProperty(second, Smi::FromInt(2), NONE);

  obj->SetElement(0, first);
  obj->SetElement(1, second);

  // Make the clone.
  JSObject* clone = JSObject::cast(Heap::CopyJSObject(obj));
  CHECK(clone != obj);

  CHECK_EQ(obj->GetElement(0), clone->GetElement(0));
  CHECK_EQ(obj->GetElement(1), clone->GetElement(1));

  CHECK_EQ(obj->GetProperty(first), clone->GetProperty(first));
  CHECK_EQ(obj->GetProperty(second), clone->GetProperty(second));

  // Flip the values.
  clone->SetProperty(first, Smi::FromInt(2), NONE);
  clone->SetProperty(second, Smi::FromInt(1), NONE);

  clone->SetElement(0, second);
  clone->SetElement(1, first);

  CHECK_EQ(obj->GetElement(1), clone->GetElement(0));
  CHECK_EQ(obj->GetElement(0), clone->GetElement(1));

  CHECK_EQ(obj->GetProperty(second), clone->GetProperty(first));
  CHECK_EQ(obj->GetProperty(first), clone->GetProperty(second));
}


TEST(StringAllocation) {
  InitializeVM();


  const unsigned char chars[] = { 0xe5, 0xa4, 0xa7 };
  for (int length = 0; length < 100; length++) {
    v8::HandleScope scope;
    char* non_ascii = NewArray<char>(3 * length + 1);
    char* ascii = NewArray<char>(length + 1);
    non_ascii[3 * length] = 0;
    ascii[length] = 0;
    for (int i = 0; i < length; i++) {
      ascii[i] = 'a';
      non_ascii[3 * i] = chars[0];
      non_ascii[3 * i + 1] = chars[1];
      non_ascii[3 * i + 2] = chars[2];
    }
    Handle<String> non_ascii_sym =
        Factory::LookupSymbol(Vector<const char>(non_ascii, 3 * length));
    CHECK_EQ(length, non_ascii_sym->length());
    Handle<String> ascii_sym =
        Factory::LookupSymbol(Vector<const char>(ascii, length));
    CHECK_EQ(length, ascii_sym->length());
    Handle<String> non_ascii_str =
        Factory::NewStringFromUtf8(Vector<const char>(non_ascii, 3 * length));
    non_ascii_str->Hash();
    CHECK_EQ(length, non_ascii_str->length());
    Handle<String> ascii_str =
        Factory::NewStringFromUtf8(Vector<const char>(ascii, length));
    ascii_str->Hash();
    CHECK_EQ(length, ascii_str->length());
    DeleteArray(non_ascii);
    DeleteArray(ascii);
  }
}


static int ObjectsFoundInHeap(Handle<Object> objs[], int size) {
  // Count the number of objects found in the heap.
  int found_count = 0;
  HeapIterator iterator;
  while (iterator.has_next()) {
    HeapObject* obj = iterator.next();
    CHECK(obj != NULL);
    for (int i = 0; i < size; i++) {
      if (*objs[i] == obj) {
        found_count++;
      }
    }
  }
  CHECK(!iterator.has_next());
  return found_count;
}


TEST(Iteration) {
  InitializeVM();
  v8::HandleScope scope;

  // Array of objects to scan haep for.
  const int objs_count = 6;
  Handle<Object> objs[objs_count];
  int next_objs_index = 0;

  // Allocate a JS array to OLD_POINTER_SPACE and NEW_SPACE
  objs[next_objs_index++] = Factory::NewJSArray(10);
  objs[next_objs_index++] = Factory::NewJSArray(10, TENURED);

  // Allocate a small string to OLD_DATA_SPACE and NEW_SPACE
  objs[next_objs_index++] =
      Factory::NewStringFromAscii(CStrVector("abcdefghij"));
  objs[next_objs_index++] =
      Factory::NewStringFromAscii(CStrVector("abcdefghij"), TENURED);

  // Allocate a large string (for large object space).
  int large_size = Heap::MaxObjectSizeInPagedSpace() + 1;
  char* str = new char[large_size];
  for (int i = 0; i < large_size - 1; ++i) str[i] = 'a';
  str[large_size - 1] = '\0';
  objs[next_objs_index++] =
      Factory::NewStringFromAscii(CStrVector(str), TENURED);
  delete[] str;

  // Add a Map object to look for.
  objs[next_objs_index++] = Handle<Map>(HeapObject::cast(*objs[0])->map());

  CHECK_EQ(objs_count, next_objs_index);
  CHECK_EQ(objs_count, ObjectsFoundInHeap(objs, objs_count));
}
