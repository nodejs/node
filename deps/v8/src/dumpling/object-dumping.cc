// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/dumpling/object-dumping.h"

#include <iostream>

#include "src/objects/instance-type.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/objects/tagged.h"
#include "src/strings/string-stream.h"

namespace v8::internal {

namespace {

void JSObjectFuzzingPrintInternalIndexRange(Tagged<JSObject> obj,
                                            StringStream* accumulator,
                                            int depth, bool is_fast_object) {
  Isolate* isolate = Isolate::Current();

  HandleScope scope(isolate);
  Tagged<DescriptorArray> descriptors =
      obj->map()->instance_descriptors(isolate);

  InternalIndex::Range index_range(0);

  std::optional<Tagged<NameDictionary>> dict;
  std::optional<Tagged<GlobalDictionary>> dict_global;
  std::optional<ReadOnlyRoots> roots;

  if (is_fast_object) {
    index_range = obj->map()->IterateOwnDescriptors();
  } else {
    roots = GetReadOnlyRoots();
    if (IsJSGlobalObject(*obj)) {
      dict_global = Cast<JSGlobalObject>(*obj)->global_dictionary(kAcquireLoad);
      index_range = dict_global.value()->IterateEntries();
    } else {
      dict = obj->property_dictionary();
      index_range = dict.value()->IterateEntries();
    }
  }

  bool first_property = true;
  accumulator->Add("{");

  for (InternalIndex i : index_range) {
    Handle<Name> key_name;

    PropertyDetails details = PropertyDetails::Empty();
    {
      DisallowGarbageCollection no_gc;
      Tagged<Name> key;

      if (is_fast_object) {
        key = descriptors->GetKey(i);
      } else if (dict_global.has_value()) {
        Tagged<Object> k;
        if (!dict_global.value()->ToKey(roots.value(), i, &k)) continue;
        key = Cast<Name>(k);
      } else {
        Tagged<Object> k;
        if (!dict.value()->ToKey(roots.value(), i, &k)) continue;
        key = Cast<Name>(k);
      }

      key_name = handle(key, isolate);

      if (is_fast_object) {
        details = descriptors->GetDetails(i);
      } else {
        details = dict.value()->DetailsAt(i);
      }
    }

    if (!first_property) {
      accumulator->Add(", ");
    }

    base::ScopedVector<char> name_buffer(100);
    key_name->NameShortPrint(name_buffer);
    accumulator->Add("%s", name_buffer.begin());

    std::stringstream attributes_stream;
    attributes_stream << details.attributes();
    accumulator->Add(attributes_stream.str().c_str());

    if (is_fast_object) {
      switch (details.location()) {
        case PropertyLocation::kField: {
          FieldIndex field_index = FieldIndex::ForDetails(obj->map(), details);
          accumulator->Add(DifferentialFuzzingPrint(
                               obj->RawFastPropertyAt(field_index), depth - 1)
                               .c_str());
          break;
        }
        case PropertyLocation::kDescriptor:
          accumulator->Add(DifferentialFuzzingPrint(
                               descriptors->GetStrongValue(i), depth - 1)
                               .c_str());
          break;
      }
    } else {
      accumulator->Add(
          DifferentialFuzzingPrint(dict.value()->ValueAt(i), depth - 1)
              .c_str());
    }

    first_property = false;
  }

  accumulator->Add("}");
}

void JSObjectFuzzingPrintFastProperties(Tagged<JSObject> obj,
                                        StringStream* accumulator, int depth) {
  JSObjectFuzzingPrintInternalIndexRange(obj, accumulator, depth, true);
}

void JSObjectFuzzingPrintDictProperties(Tagged<JSObject> obj,
                                        StringStream* accumulator, int depth) {
  if (!IsJSGlobalObject(obj)) {
    if (obj->property_dictionary()->Capacity() == 0) {
      accumulator->Add("{}");
    } else {
      JSObjectFuzzingPrintInternalIndexRange(obj, accumulator, depth, false);
    }
  }
}

void JSObjectFuzzingPrintPrototype(Tagged<JSObject> obj,
                                   StringStream* accumulator, int depth) {
  Tagged<Object> proto = obj->map()->prototype();

  // This is to avoid printing Object.prototype
  if (obj->map()->instance_type() == JS_OBJECT_PROTOTYPE_TYPE) {
    return;
  }

  accumulator->Add("__proto__:");
  accumulator->Add(DifferentialFuzzingPrint(proto, depth - 1).c_str());
}

void JSObjectFuzzingPrintElements(Tagged<JSObject> obj,
                                  StringStream* accumulator, int depth) {
  Isolate* isolate = Isolate::Current();
  HandleScope scope(isolate);

  DCHECK(!AllowGarbageCollection::IsAllowed());

  ElementsKind elements_kind = obj->GetElementsKind();

  auto process_elements = [](auto& elements, Isolate* isolate,
                             StringStream* accumulator,
                             std::function<std::string(int)> format_element) {
    int dump_len = elements->length();
    if (dump_len == 0) return;

    accumulator->Add("[");
    int hole_range_start = -1;
    // We output consecutive holes as hole_range_start-hole_range_end:the_hole
    for (int i = 0; i < dump_len; i++) {
      if (elements->is_the_hole(isolate, i)) {
        if (hole_range_start == -1) {
          hole_range_start = i;
        }
      } else {
        if (hole_range_start != -1) {
          accumulator->Add("%d-%d:the_hole,", hole_range_start, i - 1);
          hole_range_start = -1;
        }
        accumulator->Add("%s,", format_element(i).c_str());
      }
    }
    // We specifically not add trailing holes, because elements capacity can be
    // somewhat arbitrary.
    accumulator->Add("]");
  };

  if (elements_kind == PACKED_SMI_ELEMENTS ||
      elements_kind == HOLEY_SMI_ELEMENTS || elements_kind == PACKED_ELEMENTS ||
      elements_kind == HOLEY_ELEMENTS) {
    Tagged<FixedArray> elements = Cast<FixedArray>(obj->elements());
    process_elements(elements, isolate, accumulator, [&](int i) {
      return DifferentialFuzzingPrint(elements->get(i), depth - 1);
    });
  } else if (elements_kind == PACKED_DOUBLE_ELEMENTS ||
             elements_kind == HOLEY_DOUBLE_ELEMENTS) {
    Tagged<FixedDoubleArray> elements = Cast<FixedDoubleArray>(obj->elements());
    process_elements(elements, isolate, accumulator, [&](int i) {
      return std::to_string(elements->get_scalar(i));
    });
  }
  accumulator->Add("]");
}

void JSObjectFuzzingPrint(Tagged<JSObject> obj, int depth,
                          StringStream* accumulator) {
  InstanceType instance_type = obj->map()->instance_type();

  switch (instance_type) {
    case JS_ARRAY_TYPE: {
      accumulator->Add("<JSArray>");
      break;
    }
    case JS_BOUND_FUNCTION_TYPE: {
      accumulator->Add("<JSBoundFunction>");
      break;
    }
    case JS_WEAK_MAP_TYPE: {
      accumulator->Add("<JSWeakMap>");
      break;
    }
    case JS_WEAK_SET_TYPE: {
      accumulator->Add("<JSWeakSet>");
      break;
    }
    case JS_REG_EXP_TYPE: {
      accumulator->Add("<JSRegExp");
      Tagged<JSRegExp> regexp = Cast<JSRegExp>(obj);
      if (IsString(regexp->source())) {
        accumulator->Add(" ");
        Cast<String>(regexp->source())->StringShortPrint(accumulator);
      }
      accumulator->Add(">");

      break;
    }
    case JS_PROMISE_CONSTRUCTOR_TYPE:
    case JS_REG_EXP_CONSTRUCTOR_TYPE:
    case JS_ARRAY_CONSTRUCTOR_TYPE:
#define TYPED_ARRAY_CONSTRUCTORS_SWITCH(Type, type, TYPE, Ctype) \
  case TYPE##_TYPED_ARRAY_CONSTRUCTOR_TYPE:
      TYPED_ARRAYS(TYPED_ARRAY_CONSTRUCTORS_SWITCH)
#undef TYPED_ARRAY_CONSTRUCTORS_SWITCH
    case JS_CLASS_CONSTRUCTOR_TYPE:
    case JS_FUNCTION_TYPE: {
      Tagged<JSFunction> function = Cast<JSFunction>(obj);
      std::unique_ptr<char[]> fun_name = function->shared()->DebugNameCStr();
      if (fun_name[0] != '\0') {
        accumulator->Add("<JSFunction ");
        accumulator->Add(fun_name.get());
      } else {
        accumulator->Add("<JSFunction");
      }
      accumulator->Put('>');
      break;
    }
    case JS_GENERATOR_OBJECT_TYPE: {
      accumulator->Add("<JSGenerator>");
      break;
    }
    case JS_ASYNC_FUNCTION_OBJECT_TYPE: {
      accumulator->Add("<JSAsyncFunctionObject>");
      break;
    }
    case JS_ASYNC_GENERATOR_OBJECT_TYPE: {
      accumulator->Add("<JS AsyncGenerator>");
      break;
    }
    case JS_SHARED_ARRAY_TYPE:
      accumulator->Add("<JSSharedArray>");
      break;
    case JS_SHARED_STRUCT_TYPE:
      accumulator->Add("<JSSharedStruct>");
      break;
    case JS_ATOMICS_MUTEX_TYPE:
      accumulator->Add("<JSAtomicsMutex>");
      break;
    case JS_ATOMICS_CONDITION_TYPE:
      accumulator->Add("<JSAtomicsCondition>");
      break;
    case JS_MESSAGE_OBJECT_TYPE:
      accumulator->Add("<JSMessageObject>");
      break;
    case JS_EXTERNAL_OBJECT_TYPE:
      accumulator->Add("<JSExternalObject>");
      break;
    case CPP_HEAP_EXTERNAL_OBJECT_TYPE:
      accumulator->Add("<CppHeapExternalObject>");
      break;

    default: {
      Tagged<Map> map_of_this = obj->map();
      Tagged<Object> constructor = map_of_this->GetConstructor();
      bool printed = false;
      bool is_global_proxy = IsJSGlobalProxy(obj);
      if (IsJSFunction(constructor)) {
        Tagged<SharedFunctionInfo> sfi =
            Cast<JSFunction>(constructor)->shared();
        Tagged<String> constructor_name = sfi->Name();
        if (constructor_name->length() > 0) {
          accumulator->Add(is_global_proxy ? "<GlobalObject " : "<");
          accumulator->Put(constructor_name);
          printed = true;
        }
      } else if (IsFunctionTemplateInfo(constructor)) {
        accumulator->Add("<RemoteObject>");
        printed = true;
      }
      if (!printed) {
        accumulator->Add("<JS");
        if (is_global_proxy) {
          accumulator->Add("GlobalProxy");
        } else if (IsJSGlobalObject(obj)) {
          accumulator->Add("GlobalObject");
        } else {
          accumulator->Add("Object");
        }
      }
      if (IsJSPrimitiveWrapper(obj)) {
        accumulator->Add(" value = ");
        ShortPrint(Cast<JSPrimitiveWrapper>(obj)->value(), accumulator);
      }
      accumulator->Put('>');
      break;
    }
  }

  Isolate* isolate = Isolate::Current();

  if (depth > 0 && !IsUninitializedHole(obj, isolate)) {
    if (IsJSObject(obj)) {
      if (obj->HasFastProperties()) {
        JSObjectFuzzingPrintFastProperties(obj, accumulator, depth);
      } else {
        JSObjectFuzzingPrintDictProperties(obj, accumulator, depth);
      }
      JSObjectFuzzingPrintPrototype(obj, accumulator, depth);
      JSObjectFuzzingPrintElements(obj, accumulator, depth);
    }
  }
}

void HeapObjectFuzzingPrint(Tagged<HeapObject> obj, int depth,
                            std::ostream& os) {
  PtrComprCageBase cage_base = GetPtrComprCageBase();

  if (IsString(obj, cage_base)) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    Cast<String>(obj)->StringShortPrint(&accumulator);
    os << accumulator.ToCString().get();
    return;
  }
  if (IsJSObject(obj, cage_base)) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    JSObjectFuzzingPrint(Cast<JSObject>(obj), depth, &accumulator);
    os << accumulator.ToCString().get();
    return;
  }

  InstanceType instance_type = obj->map(cage_base)->instance_type();

  // Skip invalid trusted objects. Technically it'd be fine to still handle
  // them below since we only print the objects, but such an object will
  // quickly lead to out-of-sandbox segfaults and so fuzzers will complain.
  if (InstanceTypeChecker::IsTrustedObject(instance_type) &&
      !OutsideSandboxOrInReadonlySpace(obj)) {
    os << "<Invalid TrustedObject (outside trusted space)>\n";
    return;
  }

  switch (instance_type) {
    case MAP_TYPE: {
      Tagged<Map> map = Cast<Map>(obj);
      if (map->instance_type() == MAP_TYPE) {
        // This is one of the meta maps, print only relevant fields.
        os << "<MetaMap (" << Brief(map->native_context_or_null()) << ")>";
      } else {
        os << "<Map";
        os << "(";
        if (IsJSObjectMap(map)) {
          os << ElementsKindToString(map->elements_kind());
        } else {
          os << map->instance_type();
        }
        os << ")>";
      }
    } break;
    case CATCH_CONTEXT_TYPE:
      os << "<CatchContext[" << Cast<Context>(obj)->length() << "]>";
      break;
    case NATIVE_CONTEXT_TYPE:
      os << "<NativeContext[" << Cast<Context>(obj)->length() << "]>";
      break;
    case WITH_CONTEXT_TYPE:
      os << "<WithContext[" << Cast<Context>(obj)->length() << "]>";
      break;
    case FIXED_ARRAY_TYPE:
      os << "<FixedArray[" << Cast<FixedArray>(obj)->length() << "]>";
      break;
    case HOLE_TYPE: {
#define PRINT_HOLE(Type, Value, _) \
  if (Is##Type(obj)) {             \
    os << "<" #Value ">";          \
    break;                         \
  }
      HOLE_LIST(PRINT_HOLE)
#undef PRINT_HOLE
      UNREACHABLE();
    }
    case ODDBALL_TYPE: {
      if (IsUndefined(obj)) {
        os << "<undefined>";
      } else if (IsNull(obj)) {
        os << "<null>";
      } else if (IsTrue(obj)) {
        os << "<true>";
      } else if (IsFalse(obj)) {
        os << "<false>";
      } else {
        os << "<Odd Oddball: ";
        os << Cast<Oddball>(obj)->to_string()->ToCString().get();
        os << ">";
      }
      break;
    }
    case ACCESSOR_INFO_TYPE: {
      Tagged<AccessorInfo> info = Cast<AccessorInfo>(obj);
      os << "<AccessorInfo ";
      os << "name= " << Brief(info->name());
      os << ">";
      break;
    }
    case ACCESSOR_PAIR_TYPE: {
      os << "<AccessorPair>";
      break;
    }
    default:
      // TODO(mdanylo): add more cases after a test run with Fuzzilli
      FATAL("Unexpected value in switch: %s\n",
            ToString(instance_type).c_str());
  }
}

}  // namespace

void DifferentialFuzzingPrint(Tagged<Object> obj, std::ostream& os) {
  os << DifferentialFuzzingPrint(obj, v8_flags.dumpling_depth);
}

std::string DifferentialFuzzingPrint(Tagged<Object> obj, int depth) {
  std::ostringstream os;

  Tagged<HeapObject> heap_object;

  DCHECK(!obj.IsCleared());

  if (!IsAnyHole(obj) && IsNumber(obj)) {
    static const int kBufferSize = 100;
    char chars[kBufferSize];
    base::Vector<char> buffer(chars, kBufferSize);
    if (IsSmi(obj)) {
      os << IntToStringView(obj.ToSmi().value(), buffer);
    } else {
      double number = Cast<HeapNumber>(obj)->value();
      os << DoubleToStringView(number, buffer);
    }
  } else if (obj.GetHeapObjectIfWeak(&heap_object)) {
    os << "[weak] ";
    HeapObjectFuzzingPrint(heap_object, depth, os);
  } else if (obj.GetHeapObjectIfStrong(&heap_object)) {
    HeapObjectFuzzingPrint(heap_object, depth, os);
  } else {
    UNREACHABLE();
  }

  return os.str();
}

}  // namespace v8::internal
