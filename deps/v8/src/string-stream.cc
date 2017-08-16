// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/string-stream.h"

#include <memory>

#include "src/handles-inl.h"
#include "src/log.h"
#include "src/objects-inl.h"
#include "src/prototype.h"

namespace v8 {
namespace internal {

static const int kMentionedObjectCacheMaxSize = 256;

char* HeapStringAllocator::allocate(unsigned bytes) {
  space_ = NewArray<char>(bytes);
  return space_;
}


char* FixedStringAllocator::allocate(unsigned bytes) {
  CHECK_LE(bytes, length_);
  return buffer_;
}


char* FixedStringAllocator::grow(unsigned* old) {
  *old = length_;
  return buffer_;
}


bool StringStream::Put(char c) {
  if (full()) return false;
  DCHECK(length_ < capacity_);
  // Since the trailing '\0' is not accounted for in length_ fullness is
  // indicated by a difference of 1 between length_ and capacity_. Thus when
  // reaching a difference of 2 we need to grow the buffer.
  if (length_ == capacity_ - 2) {
    unsigned new_capacity = capacity_;
    char* new_buffer = allocator_->grow(&new_capacity);
    if (new_capacity > capacity_) {
      capacity_ = new_capacity;
      buffer_ = new_buffer;
    } else {
      // Reached the end of the available buffer.
      DCHECK(capacity_ >= 5);
      length_ = capacity_ - 1;  // Indicate fullness of the stream.
      buffer_[length_ - 4] = '.';
      buffer_[length_ - 3] = '.';
      buffer_[length_ - 2] = '.';
      buffer_[length_ - 1] = '\n';
      buffer_[length_] = '\0';
      return false;
    }
  }
  buffer_[length_] = c;
  buffer_[length_ + 1] = '\0';
  length_++;
  return true;
}


// A control character is one that configures a format element.  For
// instance, in %.5s, .5 are control characters.
static bool IsControlChar(char c) {
  switch (c) {
  case '0': case '1': case '2': case '3': case '4': case '5':
  case '6': case '7': case '8': case '9': case '.': case '-':
    return true;
  default:
    return false;
  }
}


void StringStream::Add(Vector<const char> format, Vector<FmtElm> elms) {
  // If we already ran out of space then return immediately.
  if (full()) return;
  int offset = 0;
  int elm = 0;
  while (offset < format.length()) {
    if (format[offset] != '%' || elm == elms.length()) {
      Put(format[offset]);
      offset++;
      continue;
    }
    // Read this formatting directive into a temporary buffer
    EmbeddedVector<char, 24> temp;
    int format_length = 0;
    // Skip over the whole control character sequence until the
    // format element type
    temp[format_length++] = format[offset++];
    while (offset < format.length() && IsControlChar(format[offset]))
      temp[format_length++] = format[offset++];
    if (offset >= format.length())
      return;
    char type = format[offset];
    temp[format_length++] = type;
    temp[format_length] = '\0';
    offset++;
    FmtElm current = elms[elm++];
    switch (type) {
    case 's': {
      DCHECK_EQ(FmtElm::C_STR, current.type_);
      const char* value = current.data_.u_c_str_;
      Add(value);
      break;
    }
    case 'w': {
      DCHECK_EQ(FmtElm::LC_STR, current.type_);
      Vector<const uc16> value = *current.data_.u_lc_str_;
      for (int i = 0; i < value.length(); i++)
        Put(static_cast<char>(value[i]));
      break;
    }
    case 'o': {
      DCHECK_EQ(FmtElm::OBJ, current.type_);
      Object* obj = current.data_.u_obj_;
      PrintObject(obj);
      break;
    }
    case 'k': {
      DCHECK_EQ(FmtElm::INT, current.type_);
      int value = current.data_.u_int_;
      if (0x20 <= value && value <= 0x7F) {
        Put(value);
      } else if (value <= 0xff) {
        Add("\\x%02x", value);
      } else {
        Add("\\u%04x", value);
      }
      break;
    }
    case 'i': case 'd': case 'u': case 'x': case 'c': case 'X': {
      int value = current.data_.u_int_;
      EmbeddedVector<char, 24> formatted;
      int length = SNPrintF(formatted, temp.start(), value);
      Add(Vector<const char>(formatted.start(), length));
      break;
    }
    case 'f': case 'g': case 'G': case 'e': case 'E': {
      double value = current.data_.u_double_;
      int inf = std::isinf(value);
      if (inf == -1) {
        Add("-inf");
      } else if (inf == 1) {
        Add("inf");
      } else if (std::isnan(value)) {
        Add("nan");
      } else {
        EmbeddedVector<char, 28> formatted;
        SNPrintF(formatted, temp.start(), value);
        Add(formatted.start());
      }
      break;
    }
    case 'p': {
      void* value = current.data_.u_pointer_;
      EmbeddedVector<char, 20> formatted;
      SNPrintF(formatted, temp.start(), value);
      Add(formatted.start());
      break;
    }
    default:
      UNREACHABLE();
      break;
    }
  }

  // Verify that the buffer is 0-terminated
  DCHECK(buffer_[length_] == '\0');
}


void StringStream::PrintObject(Object* o) {
  o->ShortPrint(this);
  if (o->IsString()) {
    if (String::cast(o)->length() <= String::kMaxShortPrintLength) {
      return;
    }
  } else if (o->IsNumber() || o->IsOddball()) {
    return;
  }
  if (o->IsHeapObject() && object_print_mode_ == kPrintObjectVerbose) {
    HeapObject* ho = HeapObject::cast(o);
    DebugObjectCache* debug_object_cache = ho->GetIsolate()->
        string_stream_debug_object_cache();
    for (size_t i = 0; i < debug_object_cache->size(); i++) {
      if ((*debug_object_cache)[i] == o) {
        Add("#%d#", static_cast<int>(i));
        return;
      }
    }
    if (debug_object_cache->size() < kMentionedObjectCacheMaxSize) {
      Add("#%d#", static_cast<int>(debug_object_cache->size()));
      debug_object_cache->push_back(HeapObject::cast(o));
    } else {
      Add("@%p", o);
    }
  }
}


std::unique_ptr<char[]> StringStream::ToCString() const {
  char* str = NewArray<char>(length_ + 1);
  MemCopy(str, buffer_, length_);
  str[length_] = '\0';
  return std::unique_ptr<char[]>(str);
}


void StringStream::Log(Isolate* isolate) {
  LOG(isolate, StringEvent("StackDump", buffer_));
}


void StringStream::OutputToFile(FILE* out) {
  // Dump the output to stdout, but make sure to break it up into
  // manageable chunks to avoid losing parts of the output in the OS
  // printing code. This is a problem on Windows in particular; see
  // the VPrint() function implementations in platform-win32.cc.
  unsigned position = 0;
  for (unsigned next; (next = position + 2048) < length_; position = next) {
    char save = buffer_[next];
    buffer_[next] = '\0';
    internal::PrintF(out, "%s", &buffer_[position]);
    buffer_[next] = save;
  }
  internal::PrintF(out, "%s", &buffer_[position]);
}


Handle<String> StringStream::ToString(Isolate* isolate) {
  return isolate->factory()->NewStringFromUtf8(
      Vector<const char>(buffer_, length_)).ToHandleChecked();
}


void StringStream::ClearMentionedObjectCache(Isolate* isolate) {
  isolate->set_string_stream_current_security_token(NULL);
  if (isolate->string_stream_debug_object_cache() == NULL) {
    isolate->set_string_stream_debug_object_cache(new DebugObjectCache());
  }
  isolate->string_stream_debug_object_cache()->clear();
}


#ifdef DEBUG
bool StringStream::IsMentionedObjectCacheClear(Isolate* isolate) {
  return object_print_mode_ == kPrintObjectConcise ||
         isolate->string_stream_debug_object_cache()->size() == 0;
}
#endif


bool StringStream::Put(String* str) {
  return Put(str, 0, str->length());
}


bool StringStream::Put(String* str, int start, int end) {
  StringCharacterStream stream(str, start);
  for (int i = start; i < end && stream.HasMore(); i++) {
    uint16_t c = stream.GetNext();
    if (c >= 127 || c < 32) {
      c = '?';
    }
    if (!Put(static_cast<char>(c))) {
      return false;  // Output was truncated.
    }
  }
  return true;
}


void StringStream::PrintName(Object* name) {
  if (name->IsString()) {
    String* str = String::cast(name);
    if (str->length() > 0) {
      Put(str);
    } else {
      Add("/* anonymous */");
    }
  } else {
    Add("%o", name);
  }
}


void StringStream::PrintUsingMap(JSObject* js_object) {
  Map* map = js_object->map();
  if (!js_object->GetHeap()->Contains(map) ||
      !map->IsHeapObject() ||
      !map->IsMap()) {
    Add("<Invalid map>\n");
    return;
  }
  int real_size = map->NumberOfOwnDescriptors();
  DescriptorArray* descs = map->instance_descriptors();
  for (int i = 0; i < real_size; i++) {
    PropertyDetails details = descs->GetDetails(i);
    if (details.location() == kField) {
      DCHECK_EQ(kData, details.kind());
      Object* key = descs->GetKey(i);
      if (key->IsString() || key->IsNumber()) {
        int len = 3;
        if (key->IsString()) {
          len = String::cast(key)->length();
        }
        for (; len < 18; len++)
          Put(' ');
        if (key->IsString()) {
          Put(String::cast(key));
        } else {
          key->ShortPrint();
        }
        Add(": ");
        FieldIndex index = FieldIndex::ForDescriptor(map, i);
        if (js_object->IsUnboxedDoubleField(index)) {
          double value = js_object->RawFastDoublePropertyAt(index);
          Add("<unboxed double> %.16g\n", FmtElm(value));
        } else {
          Object* value = js_object->RawFastPropertyAt(index);
          Add("%o\n", value);
        }
      }
    }
  }
}


void StringStream::PrintFixedArray(FixedArray* array, unsigned int limit) {
  Isolate* isolate = array->GetIsolate();
  for (unsigned int i = 0; i < 10 && i < limit; i++) {
    Object* element = array->get(i);
    if (element->IsTheHole(isolate)) continue;
    for (int len = 1; len < 18; len++) {
      Put(' ');
    }
    Add("%d: %o\n", i, array->get(i));
  }
  if (limit >= 10) {
    Add("                  ...\n");
  }
}


void StringStream::PrintByteArray(ByteArray* byte_array) {
  unsigned int limit = byte_array->length();
  for (unsigned int i = 0; i < 10 && i < limit; i++) {
    byte b = byte_array->get(i);
    Add("             %d: %3d 0x%02x", i, b, b);
    if (b >= ' ' && b <= '~') {
      Add(" '%c'", b);
    } else if (b == '\n') {
      Add(" '\n'");
    } else if (b == '\r') {
      Add(" '\r'");
    } else if (b >= 1 && b <= 26) {
      Add(" ^%c", b + 'A' - 1);
    }
    Add("\n");
  }
  if (limit >= 10) {
    Add("                  ...\n");
  }
}


void StringStream::PrintMentionedObjectCache(Isolate* isolate) {
  if (object_print_mode_ == kPrintObjectConcise) return;
  DebugObjectCache* debug_object_cache =
      isolate->string_stream_debug_object_cache();
  Add("==== Key         ============================================\n\n");
  for (size_t i = 0; i < debug_object_cache->size(); i++) {
    HeapObject* printee = (*debug_object_cache)[i];
    Add(" #%d# %p: ", static_cast<int>(i), printee);
    printee->ShortPrint(this);
    Add("\n");
    if (printee->IsJSObject()) {
      if (printee->IsJSValue()) {
        Add("           value(): %o\n", JSValue::cast(printee)->value());
      }
      PrintUsingMap(JSObject::cast(printee));
      if (printee->IsJSArray()) {
        JSArray* array = JSArray::cast(printee);
        if (array->HasFastObjectElements()) {
          unsigned int limit = FixedArray::cast(array->elements())->length();
          unsigned int length =
            static_cast<uint32_t>(JSArray::cast(array)->length()->Number());
          if (length < limit) limit = length;
          PrintFixedArray(FixedArray::cast(array->elements()), limit);
        }
      }
    } else if (printee->IsByteArray()) {
      PrintByteArray(ByteArray::cast(printee));
    } else if (printee->IsFixedArray()) {
      unsigned int limit = FixedArray::cast(printee)->length();
      PrintFixedArray(FixedArray::cast(printee), limit);
    }
  }
}


void StringStream::PrintSecurityTokenIfChanged(Object* f) {
  if (!f->IsHeapObject()) return;
  HeapObject* obj = HeapObject::cast(f);
  Isolate* isolate = obj->GetIsolate();
  Heap* heap = isolate->heap();
  if (!heap->Contains(obj)) return;
  Map* map = obj->map();
  if (!map->IsHeapObject() ||
      !heap->Contains(map) ||
      !map->IsMap() ||
      !f->IsJSFunction()) {
    return;
  }

  JSFunction* fun = JSFunction::cast(f);
  Object* perhaps_context = fun->context();
  if (perhaps_context->IsHeapObject() &&
      heap->Contains(HeapObject::cast(perhaps_context)) &&
      perhaps_context->IsContext()) {
    Context* context = fun->context();
    if (!heap->Contains(context)) {
      Add("(Function context is outside heap)\n");
      return;
    }
    Object* token = context->native_context()->security_token();
    if (token != isolate->string_stream_current_security_token()) {
      Add("Security context: %o\n", token);
      isolate->set_string_stream_current_security_token(token);
    }
  } else {
    Add("(Function context is corrupt)\n");
  }
}


void StringStream::PrintFunction(Object* f, Object* receiver, Code** code) {
  if (!f->IsHeapObject()) {
    Add("/* warning: 'function' was not a heap object */ ");
    return;
  }
  Heap* heap = HeapObject::cast(f)->GetHeap();
  if (!heap->Contains(HeapObject::cast(f))) {
    Add("/* warning: 'function' was not on the heap */ ");
    return;
  }
  if (!heap->Contains(HeapObject::cast(f)->map())) {
    Add("/* warning: function's map was not on the heap */ ");
    return;
  }
  if (!HeapObject::cast(f)->map()->IsMap()) {
    Add("/* warning: function's map was not a valid map */ ");
    return;
  }
  if (f->IsJSFunction()) {
    JSFunction* fun = JSFunction::cast(f);
    // Common case: on-stack function present and resolved.
    PrintPrototype(fun, receiver);
    *code = fun->code();
  } else if (f->IsInternalizedString()) {
    // Unresolved and megamorphic calls: Instead of the function
    // we have the function name on the stack.
    PrintName(f);
    Add("/* unresolved */ ");
  } else {
    // Unless this is the frame of a built-in function, we should always have
    // the callee function or name on the stack. If we don't, we have a
    // problem or a change of the stack frame layout.
    Add("%o", f);
    Add("/* warning: no JSFunction object or function name found */ ");
  }
}


void StringStream::PrintPrototype(JSFunction* fun, Object* receiver) {
  Object* name = fun->shared()->name();
  bool print_name = false;
  Isolate* isolate = fun->GetIsolate();
  if (receiver->IsNullOrUndefined(isolate) || receiver->IsTheHole(isolate) ||
      receiver->IsJSProxy()) {
    print_name = true;
  } else if (isolate->context() != nullptr) {
    if (!receiver->IsJSObject()) {
      receiver = receiver->GetPrototypeChainRootMap(isolate)->prototype();
    }

    for (PrototypeIterator iter(isolate, JSObject::cast(receiver),
                                kStartAtReceiver);
         !iter.IsAtEnd(); iter.Advance()) {
      if (iter.GetCurrent()->IsJSProxy()) break;
      Object* key = iter.GetCurrent<JSObject>()->SlowReverseLookup(fun);
      if (!key->IsUndefined(isolate)) {
        if (!name->IsString() ||
            !key->IsString() ||
            !String::cast(name)->Equals(String::cast(key))) {
          print_name = true;
        }
        if (name->IsString() && String::cast(name)->length() == 0) {
          print_name = false;
        }
        name = key;
        break;
      }
    }
  }
  PrintName(name);
  // Also known as - if the name in the function doesn't match the name under
  // which it was looked up.
  if (print_name) {
    Add("(aka ");
    PrintName(fun->shared()->name());
    Put(')');
  }
}


char* HeapStringAllocator::grow(unsigned* bytes) {
  unsigned new_bytes = *bytes * 2;
  // Check for overflow.
  if (new_bytes <= *bytes) {
    return space_;
  }
  char* new_space = NewArray<char>(new_bytes);
  if (new_space == NULL) {
    return space_;
  }
  MemCopy(new_space, space_, *bytes);
  *bytes = new_bytes;
  DeleteArray(space_);
  space_ = new_space;
  return new_space;
}


}  // namespace internal
}  // namespace v8
