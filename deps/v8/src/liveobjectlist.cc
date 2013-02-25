// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifdef LIVE_OBJECT_LIST

#include <ctype.h>
#include <stdlib.h>

#include "v8.h"

#include "checks.h"
#include "global-handles.h"
#include "heap.h"
#include "inspector.h"
#include "isolate.h"
#include "list-inl.h"
#include "liveobjectlist-inl.h"
#include "string-stream.h"
#include "v8utils.h"
#include "v8conversions.h"

namespace v8 {
namespace internal {


typedef int (*RawComparer)(const void*, const void*);


#ifdef CHECK_ALL_OBJECT_TYPES

#define DEBUG_LIVE_OBJECT_TYPES(v) \
  v(Smi, "unexpected: Smi") \
  \
  v(CodeCache, "unexpected: CodeCache") \
  v(BreakPointInfo, "unexpected: BreakPointInfo") \
  v(DebugInfo, "unexpected: DebugInfo") \
  v(TypeSwitchInfo, "unexpected: TypeSwitchInfo") \
  v(SignatureInfo, "unexpected: SignatureInfo") \
  v(Script, "unexpected: Script") \
  v(ObjectTemplateInfo, "unexpected: ObjectTemplateInfo") \
  v(FunctionTemplateInfo, "unexpected: FunctionTemplateInfo") \
  v(CallHandlerInfo, "unexpected: CallHandlerInfo") \
  v(InterceptorInfo, "unexpected: InterceptorInfo") \
  v(AccessCheckInfo, "unexpected: AccessCheckInfo") \
  v(AccessorInfo, "unexpected: AccessorInfo") \
  v(ExternalTwoByteString, "unexpected: ExternalTwoByteString") \
  v(ExternalAsciiString, "unexpected: ExternalAsciiString") \
  v(ExternalString, "unexpected: ExternalString") \
  v(SeqTwoByteString, "unexpected: SeqTwoByteString") \
  v(SeqAsciiString, "unexpected: SeqAsciiString") \
  v(SeqString, "unexpected: SeqString") \
  v(JSFunctionResultCache, "unexpected: JSFunctionResultCache") \
  v(NativeContext, "unexpected: NativeContext") \
  v(MapCache, "unexpected: MapCache") \
  v(CodeCacheHashTable, "unexpected: CodeCacheHashTable") \
  v(CompilationCacheTable, "unexpected: CompilationCacheTable") \
  v(SymbolTable, "unexpected: SymbolTable") \
  v(Dictionary, "unexpected: Dictionary") \
  v(HashTable, "unexpected: HashTable") \
  v(DescriptorArray, "unexpected: DescriptorArray") \
  v(ExternalFloatArray, "unexpected: ExternalFloatArray") \
  v(ExternalUnsignedIntArray, "unexpected: ExternalUnsignedIntArray") \
  v(ExternalIntArray, "unexpected: ExternalIntArray") \
  v(ExternalUnsignedShortArray, "unexpected: ExternalUnsignedShortArray") \
  v(ExternalShortArray, "unexpected: ExternalShortArray") \
  v(ExternalUnsignedByteArray, "unexpected: ExternalUnsignedByteArray") \
  v(ExternalByteArray, "unexpected: ExternalByteArray") \
  v(JSValue, "unexpected: JSValue")

#else
#define DEBUG_LIVE_OBJECT_TYPES(v)
#endif


#define FOR_EACH_LIVE_OBJECT_TYPE(v) \
  DEBUG_LIVE_OBJECT_TYPES(v) \
  \
  v(JSArray, "JSArray") \
  v(JSRegExp, "JSRegExp") \
  v(JSFunction, "JSFunction") \
  v(JSGlobalObject, "JSGlobal") \
  v(JSBuiltinsObject, "JSBuiltins") \
  v(GlobalObject, "Global") \
  v(JSGlobalProxy, "JSGlobalProxy") \
  v(JSObject, "JSObject") \
  \
  v(Context, "meta: Context") \
  v(ByteArray, "meta: ByteArray") \
  v(ExternalPixelArray, "meta: PixelArray") \
  v(ExternalArray, "meta: ExternalArray") \
  v(FixedArray, "meta: FixedArray") \
  v(String, "String") \
  v(HeapNumber, "HeapNumber") \
  \
  v(Code, "meta: Code") \
  v(Map, "meta: Map") \
  v(Oddball, "Oddball") \
  v(Foreign, "meta: Foreign") \
  v(SharedFunctionInfo, "meta: SharedFunctionInfo") \
  v(Struct, "meta: Struct") \
  \
  v(HeapObject, "HeapObject")


enum /* LiveObjectType */ {
#define DECLARE_OBJECT_TYPE_ENUM(type, name) kType##type,
  FOR_EACH_LIVE_OBJECT_TYPE(DECLARE_OBJECT_TYPE_ENUM)
  kInvalidLiveObjType,
  kNumberOfTypes
#undef DECLARE_OBJECT_TYPE_ENUM
};


LiveObjectType GetObjectType(HeapObject* heap_obj) {
  // TODO(mlam): investigate usint Map::instance_type() instead.
#define CHECK_FOR_OBJECT_TYPE(type, name) \
  if (heap_obj->Is##type()) return kType##type;
  FOR_EACH_LIVE_OBJECT_TYPE(CHECK_FOR_OBJECT_TYPE)
#undef CHECK_FOR_OBJECT_TYPE

  UNREACHABLE();
  return kInvalidLiveObjType;
}


inline const char* GetObjectTypeDesc(LiveObjectType type) {
  static const char* const name[kNumberOfTypes] = {
  #define DEFINE_OBJECT_TYPE_NAME(type, name) name,
    FOR_EACH_LIVE_OBJECT_TYPE(DEFINE_OBJECT_TYPE_NAME)
    "invalid"
  #undef DEFINE_OBJECT_TYPE_NAME
  };
  ASSERT(type < kNumberOfTypes);
  return name[type];
}


const char* GetObjectTypeDesc(HeapObject* heap_obj) {
  LiveObjectType type = GetObjectType(heap_obj);
  return GetObjectTypeDesc(type);
}


bool IsOfType(LiveObjectType type, HeapObject* obj) {
  // Note: there are types that are more general (e.g. JSObject) that would
  // have passed the Is##type_() test for more specialized types (e.g.
  // JSFunction).  If we find a more specialized match but we're looking for
  // the general type, then we should reject the ones that matches the
  // specialized type.
#define CHECK_OBJECT_TYPE(type_, name) \
  if (obj->Is##type_()) return (type == kType##type_);

  FOR_EACH_LIVE_OBJECT_TYPE(CHECK_OBJECT_TYPE)
#undef CHECK_OBJECT_TYPE

  return false;
}


const AllocationSpace kInvalidSpace = static_cast<AllocationSpace>(-1);

static AllocationSpace FindSpaceFor(String* space_str) {
  SmartArrayPointer<char> s =
      space_str->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);

  const char* key_str = *s;
  switch (key_str[0]) {
    case 'c':
      if (strcmp(key_str, "cell") == 0) return CELL_SPACE;
      if (strcmp(key_str, "code") == 0) return CODE_SPACE;
      break;
    case 'l':
      if (strcmp(key_str, "lo") == 0) return LO_SPACE;
      break;
    case 'm':
      if (strcmp(key_str, "map") == 0) return MAP_SPACE;
      break;
    case 'n':
      if (strcmp(key_str, "new") == 0) return NEW_SPACE;
      break;
    case 'o':
      if (strcmp(key_str, "old-pointer") == 0) return OLD_POINTER_SPACE;
      if (strcmp(key_str, "old-data") == 0) return OLD_DATA_SPACE;
      break;
  }
  return kInvalidSpace;
}


static bool InSpace(AllocationSpace space, HeapObject* heap_obj) {
  Heap* heap = ISOLATE->heap();
  if (space != LO_SPACE) {
    return heap->InSpace(heap_obj, space);
  }

  // This is an optimization to speed up the check for an object in the LO
  // space by exclusion because we know that all object pointers passed in
  // here are guaranteed to be in the heap.  Hence, it is safe to infer
  // using an exclusion test.
  // Note: calling Heap::InSpace(heap_obj, LO_SPACE) is too slow for our
  // filters.
  int first_space = static_cast<int>(FIRST_SPACE);
  int last_space = static_cast<int>(LO_SPACE);
  for (int sp = first_space; sp < last_space; sp++) {
    if (heap->InSpace(heap_obj, static_cast<AllocationSpace>(sp))) {
      return false;
    }
  }
  SLOW_ASSERT(heap->InSpace(heap_obj, LO_SPACE));
  return true;
}


static LiveObjectType FindTypeFor(String* type_str) {
  SmartArrayPointer<char> s =
      type_str->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);

#define CHECK_OBJECT_TYPE(type_, name) { \
    const char* type_desc = GetObjectTypeDesc(kType##type_); \
    const char* key_str = *s; \
    if (strstr(type_desc, key_str) != NULL) return kType##type_; \
  }
  FOR_EACH_LIVE_OBJECT_TYPE(CHECK_OBJECT_TYPE)
#undef CHECK_OBJECT_TYPE

  return kInvalidLiveObjType;
}


class LolFilter {
 public:
  explicit LolFilter(Handle<JSObject> filter_obj);

  inline bool is_active() const { return is_active_; }
  inline bool Matches(HeapObject* obj) {
    return !is_active() || MatchesSlow(obj);
  }

 private:
  void InitTypeFilter(Handle<JSObject> filter_obj);
  void InitSpaceFilter(Handle<JSObject> filter_obj);
  void InitPropertyFilter(Handle<JSObject> filter_obj);
  bool MatchesSlow(HeapObject* obj);

  bool is_active_;
  LiveObjectType type_;
  AllocationSpace space_;
  Handle<String> prop_;
};


LolFilter::LolFilter(Handle<JSObject> filter_obj)
    : is_active_(false),
      type_(kInvalidLiveObjType),
      space_(kInvalidSpace),
      prop_() {
  if (filter_obj.is_null()) return;

  InitTypeFilter(filter_obj);
  InitSpaceFilter(filter_obj);
  InitPropertyFilter(filter_obj);
}


void LolFilter::InitTypeFilter(Handle<JSObject> filter_obj) {
  Handle<String> type_sym = FACTORY->LookupAsciiSymbol("type");
  MaybeObject* maybe_result = filter_obj->GetProperty(*type_sym);
  Object* type_obj;
  if (maybe_result->ToObject(&type_obj)) {
    if (type_obj->IsString()) {
      String* type_str = String::cast(type_obj);
      type_ = FindTypeFor(type_str);
      if (type_ != kInvalidLiveObjType) {
        is_active_ = true;
      }
    }
  }
}


void LolFilter::InitSpaceFilter(Handle<JSObject> filter_obj) {
  Handle<String> space_sym = FACTORY->LookupAsciiSymbol("space");
  MaybeObject* maybe_result = filter_obj->GetProperty(*space_sym);
  Object* space_obj;
  if (maybe_result->ToObject(&space_obj)) {
    if (space_obj->IsString()) {
      String* space_str = String::cast(space_obj);
      space_ = FindSpaceFor(space_str);
      if (space_ != kInvalidSpace) {
        is_active_ = true;
      }
    }
  }
}


void LolFilter::InitPropertyFilter(Handle<JSObject> filter_obj) {
  Handle<String> prop_sym = FACTORY->LookupAsciiSymbol("prop");
  MaybeObject* maybe_result = filter_obj->GetProperty(*prop_sym);
  Object* prop_obj;
  if (maybe_result->ToObject(&prop_obj)) {
    if (prop_obj->IsString()) {
      prop_ = Handle<String>(String::cast(prop_obj));
      is_active_ = true;
    }
  }
}


bool LolFilter::MatchesSlow(HeapObject* obj) {
  if ((type_ != kInvalidLiveObjType) && !IsOfType(type_, obj)) {
    return false;  // Fail because obj is not of the type of interest.
  }
  if ((space_ != kInvalidSpace) && !InSpace(space_, obj)) {
    return false;  // Fail because obj is not in the space of interest.
  }
  if (!prop_.is_null() && obj->IsJSObject()) {
    LookupResult result;
    obj->Lookup(*prop_, &result);
    if (!result.IsProperty()) {
      return false;  // Fail because obj does not have the property of interest.
    }
  }
  return true;
}


class LolIterator {
 public:
  LolIterator(LiveObjectList* older, LiveObjectList* newer)
      : older_(older),
        newer_(newer),
        curr_(0),
        elements_(0),
        count_(0),
        index_(0) { }

  inline void Init() {
    SetCurrent(newer_);
    // If the elements_ list is empty, then move on to the next list as long
    // as we're not at the last list (indicated by done()).
    while ((elements_ == NULL) && !Done()) {
      SetCurrent(curr_->prev_);
    }
  }

  inline bool Done() const {
    return (curr_ == older_);
  }

  // Object level iteration.
  inline void Next() {
    index_++;
    if (index_ >= count_) {
      // Iterate backwards until we get to the oldest list.
      while (!Done()) {
        SetCurrent(curr_->prev_);
        // If we have elements to process, we're good to go.
        if (elements_ != NULL) break;

        // Else, we should advance to the next older list.
      }
    }
  }

  inline int Id() const {
    return elements_[index_].id_;
  }
  inline HeapObject* Obj() const {
    return elements_[index_].obj_;
  }

  inline int LolObjCount() const {
    if (curr_ != NULL) return curr_->obj_count_;
    return 0;
  }

 protected:
  inline void SetCurrent(LiveObjectList* new_curr) {
    curr_ = new_curr;
    if (curr_ != NULL) {
      elements_ = curr_->elements_;
      count_ = curr_->obj_count_;
      index_ = 0;
    }
  }

  LiveObjectList* older_;
  LiveObjectList* newer_;
  LiveObjectList* curr_;
  LiveObjectList::Element* elements_;
  int count_;
  int index_;
};


class LolForwardIterator : public LolIterator {
 public:
  LolForwardIterator(LiveObjectList* first, LiveObjectList* last)
      : LolIterator(first, last) {
  }

  inline void Init() {
    SetCurrent(older_);
    // If the elements_ list is empty, then move on to the next list as long
    // as we're not at the last list (indicated by Done()).
    while ((elements_ == NULL) && !Done()) {
      SetCurrent(curr_->next_);
    }
  }

  inline bool Done() const {
    return (curr_ == newer_);
  }

  // Object level iteration.
  inline void Next() {
    index_++;
    if (index_ >= count_) {
      // Done with current list.  Move on to the next.
      while (!Done()) {  // If not at the last list already, ...
        SetCurrent(curr_->next_);
        // If we have elements to process, we're good to go.
        if (elements_ != NULL) break;

        // Else, we should advance to the next list.
      }
    }
  }
};


// Minimizes the white space in a string.  Tabs and newlines are replaced
// with a space where appropriate.
static int CompactString(char* str) {
  char* src = str;
  char* dst = str;
  char prev_ch = 0;
  while (*dst != '\0') {
    char ch = *src++;
    // We will treat non-ASCII chars as '?'.
    if ((ch & 0x80) != 0) {
      ch = '?';
    }
    // Compact contiguous whitespace chars into a single ' '.
    if (isspace(ch)) {
      if (prev_ch != ' ') *dst++ = ' ';
      prev_ch = ' ';
      continue;
    }
    *dst++ = ch;
    prev_ch = ch;
  }
  return (dst - str);
}


// Generates a custom description based on the specific type of
// object we're looking at.  We only generate specialized
// descriptions where we can.  In all other cases, we emit the
// generic info.
static void GenerateObjectDesc(HeapObject* obj,
                               char* buffer,
                               int buffer_size) {
  Vector<char> buffer_v(buffer, buffer_size);
  ASSERT(obj != NULL);
  if (obj->IsJSArray()) {
    JSArray* jsarray = JSArray::cast(obj);
    double length = jsarray->length()->Number();
    OS::SNPrintF(buffer_v,
                 "%p <%s> len %g",
                 reinterpret_cast<void*>(obj),
                 GetObjectTypeDesc(obj),
                 length);

  } else if (obj->IsString()) {
    String* str = String::cast(obj);
    // Only grab up to 160 chars in case they are double byte.
    // We'll only dump 80 of them after we compact them.
    const int kMaxCharToDump = 80;
    const int kMaxBufferSize = kMaxCharToDump * 2;
    SmartArrayPointer<char> str_sp = str->ToCString(DISALLOW_NULLS,
                                                    ROBUST_STRING_TRAVERSAL,
                                                    0,
                                                    kMaxBufferSize);
    char* str_cstr = *str_sp;
    int length = CompactString(str_cstr);
    OS::SNPrintF(buffer_v,
                 "%p <%s> '%.80s%s'",
                 reinterpret_cast<void*>(obj),
                 GetObjectTypeDesc(obj),
                 str_cstr,
                 (length > kMaxCharToDump) ? "..." : "");

  } else if (obj->IsJSFunction() || obj->IsSharedFunctionInfo()) {
    SharedFunctionInfo* sinfo;
    if (obj->IsJSFunction()) {
      JSFunction* func = JSFunction::cast(obj);
      sinfo = func->shared();
    } else {
      sinfo = SharedFunctionInfo::cast(obj);
    }

    String* name = sinfo->DebugName();
    SmartArrayPointer<char> name_sp =
        name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    char* name_cstr = *name_sp;

    HeapStringAllocator string_allocator;
    StringStream stream(&string_allocator);
    sinfo->SourceCodePrint(&stream, 50);
    SmartArrayPointer<const char> source_sp = stream.ToCString();
    const char* source_cstr = *source_sp;

    OS::SNPrintF(buffer_v,
                 "%p <%s> '%s' %s",
                 reinterpret_cast<void*>(obj),
                 GetObjectTypeDesc(obj),
                 name_cstr,
                 source_cstr);

  } else if (obj->IsFixedArray()) {
    FixedArray* fixed = FixedArray::cast(obj);

    OS::SNPrintF(buffer_v,
                 "%p <%s> len %d",
                 reinterpret_cast<void*>(obj),
                 GetObjectTypeDesc(obj),
                 fixed->length());

  } else {
    OS::SNPrintF(buffer_v,
                 "%p <%s>",
                 reinterpret_cast<void*>(obj),
                 GetObjectTypeDesc(obj));
  }
}


// Utility function for filling in a line of detail in a verbose dump.
static bool AddObjDetail(Handle<FixedArray> arr,
                         int index,
                         int obj_id,
                         Handle<HeapObject> target,
                         const char* desc_str,
                         Handle<String> id_sym,
                         Handle<String> desc_sym,
                         Handle<String> size_sym,
                         Handle<JSObject> detail,
                         Handle<String> desc,
                         Handle<Object> error) {
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  detail = factory->NewJSObject(isolate->object_function());
  if (detail->IsFailure()) {
    error = detail;
    return false;
  }

  int size = 0;
  char buffer[512];
  if (desc_str == NULL) {
    ASSERT(!target.is_null());
    HeapObject* obj = *target;
    GenerateObjectDesc(obj, buffer, sizeof(buffer));
    desc_str = buffer;
    size = obj->Size();
  }
  desc = factory->NewStringFromAscii(CStrVector(desc_str));
  if (desc->IsFailure()) {
    error = desc;
    return false;
  }

  { MaybeObject* maybe_result = detail->SetProperty(*id_sym,
                                                    Smi::FromInt(obj_id),
                                                    NONE,
                                                    kNonStrictMode);
    if (maybe_result->IsFailure()) return false;
  }
  { MaybeObject* maybe_result = detail->SetProperty(*desc_sym,
                                                    *desc,
                                                    NONE,
                                                    kNonStrictMode);
    if (maybe_result->IsFailure()) return false;
  }
  { MaybeObject* maybe_result = detail->SetProperty(*size_sym,
                                                    Smi::FromInt(size),
                                                    NONE,
                                                    kNonStrictMode);
    if (maybe_result->IsFailure()) return false;
  }

  arr->set(index, *detail);
  return true;
}


class DumpWriter {
 public:
  virtual ~DumpWriter() {}

  virtual void ComputeTotalCountAndSize(LolFilter* filter,
                                        int* count,
                                        int* size) = 0;
  virtual bool Write(Handle<FixedArray> elements_arr,
                     int start,
                     int dump_limit,
                     LolFilter* filter,
                     Handle<Object> error) = 0;
};


class LolDumpWriter: public DumpWriter {
 public:
  LolDumpWriter(LiveObjectList* older, LiveObjectList* newer)
      : older_(older), newer_(newer) {
  }

  void ComputeTotalCountAndSize(LolFilter* filter, int* count, int* size) {
    *count = 0;
    *size = 0;

    LolIterator it(older_, newer_);
    for (it.Init(); !it.Done(); it.Next()) {
      HeapObject* heap_obj = it.Obj();
      if (!filter->Matches(heap_obj)) {
        continue;
      }

      *size += heap_obj->Size();
      (*count)++;
    }
  }

  bool Write(Handle<FixedArray> elements_arr,
             int start,
             int dump_limit,
             LolFilter* filter,
             Handle<Object> error) {
    // The lols are listed in latest to earliest.  We want to dump from
    // earliest to latest.  So, compute the last element to start with.
    int index = 0;
    int count = 0;

    Isolate* isolate = Isolate::Current();
    Factory* factory = isolate->factory();

    // Prefetch some needed symbols.
    Handle<String> id_sym = factory->LookupAsciiSymbol("id");
    Handle<String> desc_sym = factory->LookupAsciiSymbol("desc");
    Handle<String> size_sym = factory->LookupAsciiSymbol("size");

    // Fill the array with the lol object details.
    Handle<JSObject> detail;
    Handle<String> desc;
    Handle<HeapObject> target;

    LiveObjectList* first_lol = (older_ != NULL) ?
                                older_->next_ : LiveObjectList::first_;
    LiveObjectList* last_lol = (newer_ != NULL) ? newer_->next_ : NULL;

    LolForwardIterator it(first_lol, last_lol);
    for (it.Init(); !it.Done() && (index < dump_limit); it.Next()) {
      HeapObject* heap_obj = it.Obj();

      // Skip objects that have been filtered out.
      if (!filter->Matches(heap_obj)) {
        continue;
      }

      // Only report objects that are in the section of interest.
      if (count >= start) {
        target = Handle<HeapObject>(heap_obj);
        bool success = AddObjDetail(elements_arr,
                                    index++,
                                    it.Id(),
                                    target,
                                    NULL,
                                    id_sym,
                                    desc_sym,
                                    size_sym,
                                    detail,
                                    desc,
                                    error);
        if (!success) return false;
      }
      count++;
    }
    return true;
  }

 private:
  LiveObjectList* older_;
  LiveObjectList* newer_;
};


class RetainersDumpWriter: public DumpWriter {
 public:
  RetainersDumpWriter(Handle<HeapObject> target,
                      Handle<JSObject> instance_filter,
                      Handle<JSFunction> args_function)
      : target_(target),
        instance_filter_(instance_filter),
        args_function_(args_function) {
  }

  void ComputeTotalCountAndSize(LolFilter* filter, int* count, int* size) {
    Handle<FixedArray> retainers_arr;
    Handle<Object> error;

    *size = -1;
    LiveObjectList::GetRetainers(target_,
                                 instance_filter_,
                                 retainers_arr,
                                 0,
                                 Smi::kMaxValue,
                                 count,
                                 filter,
                                 NULL,
                                 *args_function_,
                                 error);
  }

  bool Write(Handle<FixedArray> elements_arr,
             int start,
             int dump_limit,
             LolFilter* filter,
             Handle<Object> error) {
    int dummy;
    int count;

    // Fill the retainer objects.
    count = LiveObjectList::GetRetainers(target_,
                                         instance_filter_,
                                         elements_arr,
                                         start,
                                         dump_limit,
                                         &dummy,
                                         filter,
                                         NULL,
                                         *args_function_,
                                         error);
    if (count < 0) {
        return false;
    }
    return true;
  }

 private:
  Handle<HeapObject> target_;
  Handle<JSObject> instance_filter_;
  Handle<JSFunction> args_function_;
};


class LiveObjectSummary {
 public:
  explicit LiveObjectSummary(LolFilter* filter)
      : total_count_(0),
        total_size_(0),
        found_root_(false),
        found_weak_root_(false),
        filter_(filter) {
    memset(counts_, 0, sizeof(counts_[0]) * kNumberOfEntries);
    memset(sizes_, 0, sizeof(sizes_[0]) * kNumberOfEntries);
  }

  void Add(HeapObject* heap_obj) {
    int size = heap_obj->Size();
    LiveObjectType type = GetObjectType(heap_obj);
    ASSERT(type != kInvalidLiveObjType);
    counts_[type]++;
    sizes_[type] += size;
    total_count_++;
    total_size_ += size;
  }

  void set_found_root() { found_root_ = true; }
  void set_found_weak_root() { found_weak_root_ = true; }

  inline int Count(LiveObjectType type) {
    return counts_[type];
  }
  inline int Size(LiveObjectType type) {
    return sizes_[type];
  }
  inline int total_count() {
    return total_count_;
  }
  inline int total_size() {
    return total_size_;
  }
  inline bool found_root() {
    return found_root_;
  }
  inline bool found_weak_root() {
    return found_weak_root_;
  }
  int GetNumberOfEntries() {
    int entries = 0;
    for (int i = 0; i < kNumberOfEntries; i++) {
      if (counts_[i]) entries++;
    }
    return entries;
  }

  inline LolFilter* filter() { return filter_; }

  static const int kNumberOfEntries = kNumberOfTypes;

 private:
  int counts_[kNumberOfEntries];
  int sizes_[kNumberOfEntries];
  int total_count_;
  int total_size_;
  bool found_root_;
  bool found_weak_root_;

  LolFilter* filter_;
};


// Abstraction for a summary writer.
class SummaryWriter {
 public:
  virtual ~SummaryWriter() {}
  virtual void Write(LiveObjectSummary* summary) = 0;
};


// A summary writer for filling in a summary of lol lists and diffs.
class LolSummaryWriter: public SummaryWriter {
 public:
  LolSummaryWriter(LiveObjectList* older_lol,
                   LiveObjectList* newer_lol)
      : older_(older_lol), newer_(newer_lol) {
  }

  void Write(LiveObjectSummary* summary) {
    LolFilter* filter = summary->filter();

    // Fill the summary with the lol object details.
    LolIterator it(older_, newer_);
    for (it.Init(); !it.Done(); it.Next()) {
      HeapObject* heap_obj = it.Obj();
      if (!filter->Matches(heap_obj)) {
        continue;
      }
      summary->Add(heap_obj);
    }
  }

 private:
  LiveObjectList* older_;
  LiveObjectList* newer_;
};


// A summary writer for filling in a retainers list.
class RetainersSummaryWriter: public SummaryWriter {
 public:
  RetainersSummaryWriter(Handle<HeapObject> target,
                         Handle<JSObject> instance_filter,
                         Handle<JSFunction> args_function)
      : target_(target),
        instance_filter_(instance_filter),
        args_function_(args_function) {
  }

  void Write(LiveObjectSummary* summary) {
    Handle<FixedArray> retainers_arr;
    Handle<Object> error;
    int dummy_total_count;
    LiveObjectList::GetRetainers(target_,
                                 instance_filter_,
                                 retainers_arr,
                                 0,
                                 Smi::kMaxValue,
                                 &dummy_total_count,
                                 summary->filter(),
                                 summary,
                                 *args_function_,
                                 error);
  }

 private:
  Handle<HeapObject> target_;
  Handle<JSObject> instance_filter_;
  Handle<JSFunction> args_function_;
};


uint32_t LiveObjectList::next_element_id_ = 1;
int LiveObjectList::list_count_ = 0;
int LiveObjectList::last_id_ = 0;
LiveObjectList* LiveObjectList::first_ = NULL;
LiveObjectList* LiveObjectList::last_ = NULL;


LiveObjectList::LiveObjectList(LiveObjectList* prev, int capacity)
    : prev_(prev),
      next_(NULL),
      capacity_(capacity),
      obj_count_(0) {
  elements_ = NewArray<Element>(capacity);
  id_ = ++last_id_;

  list_count_++;
}


LiveObjectList::~LiveObjectList() {
  DeleteArray<Element>(elements_);
  delete prev_;
}


int LiveObjectList::GetTotalObjCountAndSize(int* size_p) {
  int size = 0;
  int count = 0;
  LiveObjectList* lol = this;
  do {
    // Only compute total size if requested i.e. when size_p is not null.
    if (size_p != NULL) {
      Element* elements = lol->elements_;
      for (int i = 0; i < lol->obj_count_; i++) {
        HeapObject* heap_obj = elements[i].obj_;
        size += heap_obj->Size();
      }
    }
    count += lol->obj_count_;
    lol = lol->prev_;
  } while (lol != NULL);

  if (size_p != NULL) {
    *size_p = size;
  }
  return count;
}


// Adds an object to the lol.
// Returns true if successful, else returns false.
bool LiveObjectList::Add(HeapObject* obj) {
  // If the object is already accounted for in the prev list which we inherit
  // from, then no need to add it to this list.
  if ((prev() != NULL) && (prev()->Find(obj) != NULL)) {
    return true;
  }
  ASSERT(obj_count_ <= capacity_);
  if (obj_count_ == capacity_) {
    // The heap must have grown and we have more objects than capacity to store
    // them.
    return false;  // Fail this addition.
  }
  Element& element = elements_[obj_count_++];
  element.id_ = next_element_id_++;
  element.obj_ = obj;
  return true;
}


// Comparator used for sorting and searching the lol.
int LiveObjectList::CompareElement(const Element* a, const Element* b) {
  const HeapObject* obj1 = a->obj_;
  const HeapObject* obj2 = b->obj_;
  // For lol elements, it doesn't matter which comes first if 2 elements point
  // to the same object (which gets culled later).  Hence, we only care about
  // the the greater than / less than relationships.
  return (obj1 > obj2) ? 1 : (obj1 == obj2) ? 0 : -1;
}


// Looks for the specified object in the lol, and returns its element if found.
LiveObjectList::Element* LiveObjectList::Find(HeapObject* obj) {
  LiveObjectList* lol = this;
  Element key;
  Element* result = NULL;

  key.obj_ = obj;
  // Iterate through the chain of lol's to look for the object.
  while ((result == NULL) && (lol != NULL)) {
    result = reinterpret_cast<Element*>(
        bsearch(&key, lol->elements_, lol->obj_count_,
                sizeof(Element),
                reinterpret_cast<RawComparer>(CompareElement)));
    lol = lol->prev_;
  }
  return result;
}


// "Nullifies" (convert the HeapObject* into an SMI) so that it will get cleaned
// up in the GCEpilogue, while preserving the sort order of the lol.
// NOTE: the lols need to be already sorted before NullifyMostRecent() is
// called.
void LiveObjectList::NullifyMostRecent(HeapObject* obj) {
  LiveObjectList* lol = last();
  Element key;
  Element* result = NULL;

  key.obj_ = obj;
  // Iterate through the chain of lol's to look for the object.
  while (lol != NULL) {
    result = reinterpret_cast<Element*>(
        bsearch(&key, lol->elements_, lol->obj_count_,
                sizeof(Element),
                reinterpret_cast<RawComparer>(CompareElement)));
    if (result != NULL) {
      // Since there may be more than one (we are nullifying dup's after all),
      // find the first in the current lol, and nullify that.  The lol should
      // be sorted already to make this easy (see the use of SortAll()).
      int i = result - lol->elements_;

      // NOTE: we sort the lol in increasing order.  So, if an object has been
      // "nullified" (its lowest bit will be cleared to make it look like an
      // SMI), it would/should show up before the equivalent dups that have not
      // yet been "nullified".  Hence, we should be searching backwards for the
      // first occurence of a matching object and nullify that instance.  This
      // will ensure that we preserve the expected sorting order.
      for (i--; i > 0; i--) {
        Element* element = &lol->elements_[i];
        HeapObject* curr_obj = element->obj_;
        if (curr_obj != obj) {
            break;  // No more matches.  Let's move on.
        }
        result = element;  // Let this earlier match be the result.
      }

      // Nullify the object.
      NullifyNonLivePointer(&result->obj_);
      return;
    }
    lol = lol->prev_;
  }
}


// Sorts the lol.
void LiveObjectList::Sort() {
  if (obj_count_ > 0) {
    Vector<Element> elements_v(elements_, obj_count_);
    elements_v.Sort(CompareElement);
  }
}


// Sorts all captured lols starting from the latest.
void LiveObjectList::SortAll() {
  LiveObjectList* lol = last();
  while (lol != NULL) {
    lol->Sort();
    lol = lol->prev_;
  }
}


// Counts the number of objects in the heap.
static int CountHeapObjects() {
  int count = 0;
  // Iterate over all the heap spaces and count the number of objects.
  HeapIterator iterator;
  HeapObject* heap_obj = NULL;
  while ((heap_obj = iterator.next()) != NULL) {
    count++;
  }
  return count;
}


// Captures a current snapshot of all objects in the heap.
MaybeObject* LiveObjectList::Capture() {
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  // Count the number of objects in the heap.
  int total_count = CountHeapObjects();
  int count = total_count;
  int size = 0;

  LiveObjectList* last_lol = last();
  if (last_lol != NULL) {
    count -= last_lol->TotalObjCount();
  }

  LiveObjectList* lol;

  // Create a lol large enough to track all the objects.
  lol = new LiveObjectList(last_lol, count);
  if (lol == NULL) {
    return NULL;  // No memory to proceed.
  }

  // The HeapIterator needs to be in its own scope because it disables
  // allocation, and we need allocate below.
  {
    // Iterate over all the heap spaces and add the objects.
    HeapIterator iterator;
    HeapObject* heap_obj = NULL;
    bool failed = false;
    while (!failed && (heap_obj = iterator.next()) != NULL) {
      failed = !lol->Add(heap_obj);
      size += heap_obj->Size();
    }
    ASSERT(!failed);

    lol->Sort();

    // Add the current lol to the list of lols.
    if (last_ != NULL) {
      last_->next_ = lol;
    } else {
      first_ = lol;
    }
    last_ = lol;

#ifdef VERIFY_LOL
    if (FLAG_verify_lol) {
      Verify(true);
    }
#endif
  }

  Handle<String> id_sym = factory->LookupAsciiSymbol("id");
  Handle<String> count_sym = factory->LookupAsciiSymbol("count");
  Handle<String> size_sym = factory->LookupAsciiSymbol("size");

  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());
  if (result->IsFailure()) return Object::cast(*result);

  { MaybeObject* maybe_result = result->SetProperty(*id_sym,
                                                    Smi::FromInt(lol->id()),
                                                    NONE,
                                                    kNonStrictMode);
    if (maybe_result->IsFailure()) return maybe_result;
  }
  { MaybeObject* maybe_result = result->SetProperty(*count_sym,
                                                    Smi::FromInt(total_count),
                                                    NONE,
                                                    kNonStrictMode);
    if (maybe_result->IsFailure()) return maybe_result;
  }
  { MaybeObject* maybe_result = result->SetProperty(*size_sym,
                                                    Smi::FromInt(size),
                                                    NONE,
                                                    kNonStrictMode);
    if (maybe_result->IsFailure()) return maybe_result;
  }

  return *result;
}


// Delete doesn't actually deletes an lol.  It just marks it as invisible since
// its contents are considered to be part of subsequent lists as well.  The
// only time we'll actually delete the lol is when we Reset() or if the lol is
// invisible, and its element count reaches 0.
bool LiveObjectList::Delete(int id) {
  LiveObjectList* lol = last();
  while (lol != NULL) {
    if (lol->id() == id) {
      break;
    }
    lol = lol->prev_;
  }

  // If no lol is found for this id, then we fail to delete.
  if (lol == NULL) return false;

  // Else, mark the lol as invisible i.e. id == 0.
  lol->id_ = 0;
  list_count_--;
  ASSERT(list_count_ >= 0);
  if (lol->obj_count_ == 0) {
    // Point the next lol's prev to this lol's prev.
    LiveObjectList* next = lol->next_;
    LiveObjectList* prev = lol->prev_;
    // Point next's prev to prev.
    if (next != NULL) {
      next->prev_ = lol->prev_;
    } else {
      last_ = lol->prev_;
    }
    // Point prev's next to next.
    if (prev != NULL) {
      prev->next_ = lol->next_;
    } else {
      first_ = lol->next_;
    }

    lol->prev_ = NULL;
    lol->next_ = NULL;

    // Delete this now empty and invisible lol.
    delete lol;
  }

  // Just in case we've marked everything invisible, then clean up completely.
  if (list_count_ == 0) {
    Reset();
  }

  return true;
}


MaybeObject* LiveObjectList::Dump(int older_id,
                                  int newer_id,
                                  int start_idx,
                                  int dump_limit,
                                  Handle<JSObject> filter_obj) {
  if ((older_id < 0) || (newer_id < 0) || (last() == NULL)) {
    return Failure::Exception();  // Fail: 0 is not a valid lol id.
  }
  if (newer_id < older_id) {
    // They are not in the expected order.  Swap them.
    int temp = older_id;
    older_id = newer_id;
    newer_id = temp;
  }

  LiveObjectList* newer_lol = FindLolForId(newer_id, last());
  LiveObjectList* older_lol = FindLolForId(older_id, newer_lol);

  // If the id is defined, and we can't find a LOL for it, then we have an
  // invalid id.
  if ((newer_id != 0) && (newer_lol == NULL)) {
    return Failure::Exception();  // Fail: the newer lol id is invalid.
  }
  if ((older_id != 0) && (older_lol == NULL)) {
    return Failure::Exception();  // Fail: the older lol id is invalid.
  }

  LolFilter filter(filter_obj);
  LolDumpWriter writer(older_lol, newer_lol);
  return DumpPrivate(&writer, start_idx, dump_limit, &filter);
}


MaybeObject* LiveObjectList::DumpPrivate(DumpWriter* writer,
                                         int start,
                                         int dump_limit,
                                         LolFilter* filter) {
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();

  HandleScope scope(isolate);

  // Calculate the number of entries of the dump.
  int count = -1;
  int size = -1;
  writer->ComputeTotalCountAndSize(filter, &count, &size);

  // Adjust for where to start the dump.
  if ((start < 0) || (start >= count)) {
    return Failure::Exception();  // invalid start.
  }

  int remaining_count = count - start;
  if (dump_limit > remaining_count) {
    dump_limit = remaining_count;
  }

  // Allocate an array to hold the result.
  Handle<FixedArray> elements_arr = factory->NewFixedArray(dump_limit);
  if (elements_arr->IsFailure()) return Object::cast(*elements_arr);

  // Fill in the dump.
  Handle<Object> error;
  bool success = writer->Write(elements_arr,
                               start,
                               dump_limit,
                               filter,
                               error);
  if (!success) return Object::cast(*error);

  MaybeObject* maybe_result;

  // Allocate the result body.
  Handle<JSObject> body = factory->NewJSObject(isolate->object_function());
  if (body->IsFailure()) return Object::cast(*body);

  // Set the updated body.count.
  Handle<String> count_sym = factory->LookupAsciiSymbol("count");
  maybe_result = body->SetProperty(*count_sym,
                                   Smi::FromInt(count),
                                   NONE,
                                   kNonStrictMode);
  if (maybe_result->IsFailure()) return maybe_result;

  // Set the updated body.size if appropriate.
  if (size >= 0) {
    Handle<String> size_sym = factory->LookupAsciiSymbol("size");
    maybe_result = body->SetProperty(*size_sym,
                                     Smi::FromInt(size),
                                     NONE,
                                     kNonStrictMode);
    if (maybe_result->IsFailure()) return maybe_result;
  }

  // Set body.first_index.
  Handle<String> first_sym = factory->LookupAsciiSymbol("first_index");
  maybe_result = body->SetProperty(*first_sym,
                                   Smi::FromInt(start),
                                   NONE,
                                   kNonStrictMode);
  if (maybe_result->IsFailure()) return maybe_result;

  // Allocate the JSArray of the elements.
  Handle<JSObject> elements = factory->NewJSObject(isolate->array_function());
  if (elements->IsFailure()) return Object::cast(*elements);

  maybe_result = Handle<JSArray>::cast(elements)->SetContent(*elements_arr);
  if (maybe_result->IsFailure()) return maybe_result;

  // Set body.elements.
  Handle<String> elements_sym = factory->LookupAsciiSymbol("elements");
  maybe_result = body->SetProperty(*elements_sym,
                                   *elements,
                                   NONE,
                                   kNonStrictMode);
  if (maybe_result->IsFailure()) return maybe_result;

  return *body;
}


MaybeObject* LiveObjectList::Summarize(int older_id,
                                       int newer_id,
                                       Handle<JSObject> filter_obj) {
  if ((older_id < 0) || (newer_id < 0) || (last() == NULL)) {
    return Failure::Exception();  // Fail: 0 is not a valid lol id.
  }
  if (newer_id < older_id) {
    // They are not in the expected order.  Swap them.
    int temp = older_id;
    older_id = newer_id;
    newer_id = temp;
  }

  LiveObjectList* newer_lol = FindLolForId(newer_id, last());
  LiveObjectList* older_lol = FindLolForId(older_id, newer_lol);

  // If the id is defined, and we can't find a LOL for it, then we have an
  // invalid id.
  if ((newer_id != 0) && (newer_lol == NULL)) {
    return Failure::Exception();  // Fail: the newer lol id is invalid.
  }
  if ((older_id != 0) && (older_lol == NULL)) {
    return Failure::Exception();  // Fail: the older lol id is invalid.
  }

  LolFilter filter(filter_obj);
  LolSummaryWriter writer(older_lol, newer_lol);
  return SummarizePrivate(&writer, &filter, false);
}


// Creates a summary report for the debugger.
// Note: the SummaryWriter takes care of iterating over objects and filling in
// the summary.
MaybeObject* LiveObjectList::SummarizePrivate(SummaryWriter* writer,
                                              LolFilter* filter,
                                              bool is_tracking_roots) {
  HandleScope scope;
  MaybeObject* maybe_result;

  LiveObjectSummary summary(filter);
  writer->Write(&summary);

  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();

  // The result body will look like this:
  // body: {
  //   count: <total_count>,
  //   size: <total_size>,
  //   found_root: <boolean>,       // optional.
  //   found_weak_root: <boolean>,  // optional.
  //   summary: [
  //     {
  //       desc: "<object type name>",
  //       count: <count>,
  //       size: size
  //     },
  //     ...
  //   ]
  // }

  // Prefetch some needed symbols.
  Handle<String> desc_sym = factory->LookupAsciiSymbol("desc");
  Handle<String> count_sym = factory->LookupAsciiSymbol("count");
  Handle<String> size_sym = factory->LookupAsciiSymbol("size");
  Handle<String> summary_sym = factory->LookupAsciiSymbol("summary");

  // Allocate the summary array.
  int entries_count = summary.GetNumberOfEntries();
  Handle<FixedArray> summary_arr =
      factory->NewFixedArray(entries_count);
  if (summary_arr->IsFailure()) return Object::cast(*summary_arr);

  int idx = 0;
  for (int i = 0; i < LiveObjectSummary::kNumberOfEntries; i++) {
    // Allocate the summary record.
    Handle<JSObject> detail = factory->NewJSObject(isolate->object_function());
    if (detail->IsFailure()) return Object::cast(*detail);

    // Fill in the summary record.
    LiveObjectType type = static_cast<LiveObjectType>(i);
    int count = summary.Count(type);
    if (count) {
      const char* desc_cstr = GetObjectTypeDesc(type);
      Handle<String> desc = factory->LookupAsciiSymbol(desc_cstr);
      int size = summary.Size(type);

      maybe_result = detail->SetProperty(*desc_sym,
                                         *desc,
                                         NONE,
                                         kNonStrictMode);
      if (maybe_result->IsFailure()) return maybe_result;
      maybe_result = detail->SetProperty(*count_sym,
                                         Smi::FromInt(count),
                                         NONE,
                                         kNonStrictMode);
      if (maybe_result->IsFailure()) return maybe_result;
      maybe_result = detail->SetProperty(*size_sym,
                                         Smi::FromInt(size),
                                         NONE,
                                         kNonStrictMode);
      if (maybe_result->IsFailure()) return maybe_result;

      summary_arr->set(idx++, *detail);
    }
  }

  // Wrap the summary fixed array in a JS array.
  Handle<JSObject> summary_obj =
    factory->NewJSObject(isolate->array_function());
  if (summary_obj->IsFailure()) return Object::cast(*summary_obj);

  maybe_result = Handle<JSArray>::cast(summary_obj)->SetContent(*summary_arr);
  if (maybe_result->IsFailure()) return maybe_result;

  // Create the body object.
  Handle<JSObject> body = factory->NewJSObject(isolate->object_function());
  if (body->IsFailure()) return Object::cast(*body);

  // Fill out the body object.
  int total_count = summary.total_count();
  int total_size = summary.total_size();
  maybe_result = body->SetProperty(*count_sym,
                                   Smi::FromInt(total_count),
                                   NONE,
                                   kNonStrictMode);
  if (maybe_result->IsFailure()) return maybe_result;

  maybe_result = body->SetProperty(*size_sym,
                                   Smi::FromInt(total_size),
                                   NONE,
                                   kNonStrictMode);
  if (maybe_result->IsFailure()) return maybe_result;

  if (is_tracking_roots) {
    int found_root = summary.found_root();
    int found_weak_root = summary.found_weak_root();
    Handle<String> root_sym = factory->LookupAsciiSymbol("found_root");
    Handle<String> weak_root_sym =
        factory->LookupAsciiSymbol("found_weak_root");
    maybe_result = body->SetProperty(*root_sym,
                                     Smi::FromInt(found_root),
                                     NONE,
                                     kNonStrictMode);
    if (maybe_result->IsFailure()) return maybe_result;
    maybe_result = body->SetProperty(*weak_root_sym,
                                     Smi::FromInt(found_weak_root),
                                     NONE,
                                     kNonStrictMode);
    if (maybe_result->IsFailure()) return maybe_result;
  }

  maybe_result = body->SetProperty(*summary_sym,
                                   *summary_obj,
                                   NONE,
                                   kNonStrictMode);
  if (maybe_result->IsFailure()) return maybe_result;

  return *body;
}


// Returns an array listing the captured lols.
// Note: only dumps the section starting at start_idx and only up to
// dump_limit entries.
MaybeObject* LiveObjectList::Info(int start_idx, int dump_limit) {
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();

  HandleScope scope(isolate);
  MaybeObject* maybe_result;

  int total_count = LiveObjectList::list_count();
  int dump_count = total_count;

  // Adjust for where to start the dump.
  if (total_count == 0) {
      start_idx = 0;  // Ensure this to get an empty list.
  } else if ((start_idx < 0) || (start_idx >= total_count)) {
    return Failure::Exception();  // invalid start.
  }
  dump_count -= start_idx;

  // Adjust for the dump limit.
  if (dump_count > dump_limit) {
    dump_count = dump_limit;
  }

  // Allocate an array to hold the result.
  Handle<FixedArray> list = factory->NewFixedArray(dump_count);
  if (list->IsFailure()) return Object::cast(*list);

  // Prefetch some needed symbols.
  Handle<String> id_sym = factory->LookupAsciiSymbol("id");
  Handle<String> count_sym = factory->LookupAsciiSymbol("count");
  Handle<String> size_sym = factory->LookupAsciiSymbol("size");

  // Fill the array with the lol details.
  int idx = 0;
  LiveObjectList* lol = first_;
  while ((lol != NULL) && (idx < start_idx)) {  // Skip tail entries.
    if (lol->id() != 0) {
      idx++;
    }
    lol = lol->next();
  }
  idx = 0;
  while ((lol != NULL) && (dump_limit != 0)) {
    if (lol->id() != 0) {
      int count;
      int size;
      count = lol->GetTotalObjCountAndSize(&size);

      Handle<JSObject> detail =
          factory->NewJSObject(isolate->object_function());
      if (detail->IsFailure()) return Object::cast(*detail);

      maybe_result = detail->SetProperty(*id_sym,
                                         Smi::FromInt(lol->id()),
                                         NONE,
                                         kNonStrictMode);
      if (maybe_result->IsFailure()) return maybe_result;
      maybe_result = detail->SetProperty(*count_sym,
                                         Smi::FromInt(count),
                                         NONE,
                                         kNonStrictMode);
      if (maybe_result->IsFailure()) return maybe_result;
      maybe_result = detail->SetProperty(*size_sym,
                                         Smi::FromInt(size),
                                         NONE,
                                         kNonStrictMode);
      if (maybe_result->IsFailure()) return maybe_result;
      list->set(idx++, *detail);
      dump_limit--;
    }
    lol = lol->next();
  }

  // Return the result as a JS array.
  Handle<JSObject> lols = factory->NewJSObject(isolate->array_function());

  maybe_result = Handle<JSArray>::cast(lols)->SetContent(*list);
  if (maybe_result->IsFailure()) return maybe_result;

  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());
  if (result->IsFailure()) return Object::cast(*result);

  maybe_result = result->SetProperty(*count_sym,
                                     Smi::FromInt(total_count),
                                     NONE,
                                     kNonStrictMode);
  if (maybe_result->IsFailure()) return maybe_result;

  Handle<String> first_sym = factory->LookupAsciiSymbol("first_index");
  maybe_result = result->SetProperty(*first_sym,
                                     Smi::FromInt(start_idx),
                                     NONE,
                                     kNonStrictMode);
  if (maybe_result->IsFailure()) return maybe_result;

  Handle<String> lists_sym = factory->LookupAsciiSymbol("lists");
  maybe_result = result->SetProperty(*lists_sym,
                                     *lols,
                                     NONE,
                                     kNonStrictMode);
  if (maybe_result->IsFailure()) return maybe_result;

  return *result;
}


// Deletes all captured lols.
void LiveObjectList::Reset() {
  LiveObjectList* lol = last();
  // Just delete the last.  Each lol will delete it's prev automatically.
  delete lol;

  next_element_id_ = 1;
  list_count_ = 0;
  last_id_ = 0;
  first_ = NULL;
  last_ = NULL;
}


// Gets the object for the specified obj id.
Object* LiveObjectList::GetObj(int obj_id) {
  Element* element = FindElementFor<int>(GetElementId, obj_id);
  if (element != NULL) {
    return Object::cast(element->obj_);
  }
  return HEAP->undefined_value();
}


// Gets the obj id for the specified address if valid.
int LiveObjectList::GetObjId(Object* obj) {
  // Make a heap object pointer from the address.
  HeapObject* hobj = HeapObject::cast(obj);
  Element* element = FindElementFor<HeapObject*>(GetElementObj, hobj);
  if (element != NULL) {
    return element->id_;
  }
  return 0;  // Invalid address.
}


// Gets the obj id for the specified address if valid.
Object* LiveObjectList::GetObjId(Handle<String> address) {
  SmartArrayPointer<char> addr_str =
      address->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);

  Isolate* isolate = Isolate::Current();

  // Extract the address value from the string.
  int value =
      static_cast<int>(StringToInt(isolate->unicode_cache(), *address, 16));
  Object* obj = reinterpret_cast<Object*>(value);
  return Smi::FromInt(GetObjId(obj));
}


// Helper class for copying HeapObjects.
class LolVisitor: public ObjectVisitor {
 public:
  LolVisitor(HeapObject* target, Handle<HeapObject> handle_to_skip)
      : target_(target), handle_to_skip_(handle_to_skip), found_(false) {}

  void VisitPointer(Object** p) { CheckPointer(p); }

  void VisitPointers(Object** start, Object** end) {
    // Check all HeapObject pointers in [start, end).
    for (Object** p = start; !found() && p < end; p++) CheckPointer(p);
  }

  inline bool found() const { return found_; }
  inline bool reset() { return found_ = false; }

 private:
  inline void CheckPointer(Object** p) {
    Object* object = *p;
    if (HeapObject::cast(object) == target_) {
      // We may want to skip this handle because the handle may be a local
      // handle in a handle scope in one of our callers.  Once we return,
      // that handle will be popped.  Hence, we don't want to count it as
      // a root that would have kept the target object alive.
      if (!handle_to_skip_.is_null() &&
          handle_to_skip_.location() == reinterpret_cast<HeapObject**>(p)) {
        return;  // Skip this handle.
      }
      found_ = true;
    }
  }

  HeapObject* target_;
  Handle<HeapObject> handle_to_skip_;
  bool found_;
};


inline bool AddRootRetainerIfFound(const LolVisitor& visitor,
                                   LolFilter* filter,
                                   LiveObjectSummary* summary,
                                   void (*SetRootFound)(LiveObjectSummary* s),
                                   int start,
                                   int dump_limit,
                                   int* total_count,
                                   Handle<FixedArray> retainers_arr,
                                   int* count,
                                   int* index,
                                   const char* root_name,
                                   Handle<String> id_sym,
                                   Handle<String> desc_sym,
                                   Handle<String> size_sym,
                                   Handle<Object> error) {
  HandleScope scope;

  // Scratch handles.
  Handle<JSObject> detail;
  Handle<String> desc;
  Handle<HeapObject> retainer;

  if (visitor.found()) {
    if (!filter->is_active()) {
      (*total_count)++;
      if (summary) {
        SetRootFound(summary);
      } else if ((*total_count > start) && ((*index) < dump_limit)) {
        (*count)++;
        if (!retainers_arr.is_null()) {
          return AddObjDetail(retainers_arr,
                              (*index)++,
                              0,
                              retainer,
                              root_name,
                              id_sym,
                              desc_sym,
                              size_sym,
                              detail,
                              desc,
                              error);
        }
      }
    }
  }
  return true;
}


inline void SetFoundRoot(LiveObjectSummary* summary) {
  summary->set_found_root();
}


inline void SetFoundWeakRoot(LiveObjectSummary* summary) {
  summary->set_found_weak_root();
}


int LiveObjectList::GetRetainers(Handle<HeapObject> target,
                                 Handle<JSObject> instance_filter,
                                 Handle<FixedArray> retainers_arr,
                                 int start,
                                 int dump_limit,
                                 int* total_count,
                                 LolFilter* filter,
                                 LiveObjectSummary* summary,
                                 JSFunction* arguments_function,
                                 Handle<Object> error) {
  HandleScope scope;

  // Scratch handles.
  Handle<JSObject> detail;
  Handle<String> desc;
  Handle<HeapObject> retainer;

  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();

  // Prefetch some needed symbols.
  Handle<String> id_sym = factory->LookupAsciiSymbol("id");
  Handle<String> desc_sym = factory->LookupAsciiSymbol("desc");
  Handle<String> size_sym = factory->LookupAsciiSymbol("size");

  NoHandleAllocation ha;
  int count = 0;
  int index = 0;
  Handle<JSObject> last_obj;

  *total_count = 0;

  // Iterate roots.
  LolVisitor lol_visitor(*target, target);
  isolate->heap()->IterateStrongRoots(&lol_visitor, VISIT_ALL);
  if (!AddRootRetainerIfFound(lol_visitor,
                              filter,
                              summary,
                              SetFoundRoot,
                              start,
                              dump_limit,
                              total_count,
                              retainers_arr,
                              &count,
                              &index,
                              "<root>",
                              id_sym,
                              desc_sym,
                              size_sym,
                              error)) {
    return -1;
  }

  lol_visitor.reset();
  isolate->heap()->IterateWeakRoots(&lol_visitor, VISIT_ALL);
  if (!AddRootRetainerIfFound(lol_visitor,
                              filter,
                              summary,
                              SetFoundWeakRoot,
                              start,
                              dump_limit,
                              total_count,
                              retainers_arr,
                              &count,
                              &index,
                              "<weak root>",
                              id_sym,
                              desc_sym,
                              size_sym,
                              error)) {
    return -1;
  }

  // Iterate the live object lists.
  LolIterator it(NULL, last());
  for (it.Init(); !it.Done() && (index < dump_limit); it.Next()) {
    HeapObject* heap_obj = it.Obj();

    // Only look at all JSObjects.
    if (heap_obj->IsJSObject()) {
      // Skip context extension objects and argument arrays as these are
      // checked in the context of functions using them.
      JSObject* obj = JSObject::cast(heap_obj);
      if (obj->IsJSContextExtensionObject() ||
          obj->map()->constructor() == arguments_function) {
        continue;
      }

      // Check if the JS object has a reference to the object looked for.
      if (obj->ReferencesObject(*target)) {
        // Check instance filter if supplied. This is normally used to avoid
        // references from mirror objects (see Runtime_IsInPrototypeChain).
        if (!instance_filter->IsUndefined()) {
          Object* V = obj;
          while (true) {
            Object* prototype = V->GetPrototype();
            if (prototype->IsNull()) {
              break;
            }
            if (*instance_filter == prototype) {
              obj = NULL;  // Don't add this object.
              break;
            }
            V = prototype;
          }
        }

        if (obj != NULL) {
          // Skip objects that have been filtered out.
          if (filter->Matches(heap_obj)) {
            continue;
          }

          // Valid reference found add to instance array if supplied an update
          // count.
          last_obj = Handle<JSObject>(obj);
          (*total_count)++;

          if (summary != NULL) {
            summary->Add(heap_obj);
          } else if ((*total_count > start) && (index < dump_limit)) {
            count++;
            if (!retainers_arr.is_null()) {
              retainer = Handle<HeapObject>(heap_obj);
              bool success = AddObjDetail(retainers_arr,
                                          index++,
                                          it.Id(),
                                          retainer,
                                          NULL,
                                          id_sym,
                                          desc_sym,
                                          size_sym,
                                          detail,
                                          desc,
                                          error);
              if (!success) return -1;
            }
          }
        }
      }
    }
  }

  // Check for circular reference only. This can happen when the object is only
  // referenced from mirrors and has a circular reference in which case the
  // object is not really alive and would have been garbage collected if not
  // referenced from the mirror.

  if (*total_count == 1 && !last_obj.is_null() && *last_obj == *target) {
    count = 0;
    *total_count = 0;
  }

  return count;
}


MaybeObject* LiveObjectList::GetObjRetainers(int obj_id,
                                             Handle<JSObject> instance_filter,
                                             bool verbose,
                                             int start,
                                             int dump_limit,
                                             Handle<JSObject> filter_obj) {
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope scope(isolate);

  // Get the target object.
  HeapObject* heap_obj = HeapObject::cast(GetObj(obj_id));
  if (heap_obj == heap->undefined_value()) {
    return heap_obj;
  }

  Handle<HeapObject> target = Handle<HeapObject>(heap_obj);

  // Get the constructor function for context extension and arguments array.
  JSObject* arguments_boilerplate =
      isolate->context()->native_context()->arguments_boilerplate();
  JSFunction* arguments_function =
      JSFunction::cast(arguments_boilerplate->map()->constructor());

  Handle<JSFunction> args_function = Handle<JSFunction>(arguments_function);
  LolFilter filter(filter_obj);

  if (!verbose) {
    RetainersSummaryWriter writer(target, instance_filter, args_function);
    return SummarizePrivate(&writer, &filter, true);

  } else {
    RetainersDumpWriter writer(target, instance_filter, args_function);
    Object* body_obj;
    MaybeObject* maybe_result =
        DumpPrivate(&writer, start, dump_limit, &filter);
    if (!maybe_result->ToObject(&body_obj)) {
      return maybe_result;
    }

    // Set body.id.
    Handle<JSObject> body = Handle<JSObject>(JSObject::cast(body_obj));
    Handle<String> id_sym = factory->LookupAsciiSymbol("id");
    maybe_result = body->SetProperty(*id_sym,
                                     Smi::FromInt(obj_id),
                                     NONE,
                                     kNonStrictMode);
    if (maybe_result->IsFailure()) return maybe_result;

    return *body;
  }
}


Object* LiveObjectList::PrintObj(int obj_id) {
  Object* obj = GetObj(obj_id);
  if (!obj) {
    return HEAP->undefined_value();
  }

  EmbeddedVector<char, 128> temp_filename;
  static int temp_count = 0;
  const char* path_prefix = ".";

  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  if (FLAG_lol_workdir) {
    path_prefix = FLAG_lol_workdir;
  }
  OS::SNPrintF(temp_filename, "%s/lol-print-%d", path_prefix, ++temp_count);

  FILE* f = OS::FOpen(temp_filename.start(), "w+");

  PrintF(f, "@%d ", LiveObjectList::GetObjId(obj));
#ifdef OBJECT_PRINT
#ifdef INSPECTOR
  Inspector::DumpObjectType(f, obj);
#endif  // INSPECTOR
  PrintF(f, "\n");
  obj->Print(f);
#else  // !OBJECT_PRINT
  obj->ShortPrint(f);
#endif  // !OBJECT_PRINT
  PrintF(f, "\n");
  Flush(f);
  fclose(f);

  // Create a string from the temp_file.
  // Note: the mmapped resource will take care of closing the file.
  MemoryMappedExternalResource* resource =
      new MemoryMappedExternalResource(temp_filename.start(), true);
  if (resource->exists() && !resource->is_empty()) {
    ASSERT(resource->IsAscii());
    Handle<String> dump_string =
        factory->NewExternalStringFromAscii(resource);
    heap->external_string_table()->AddString(*dump_string);
    return *dump_string;
  } else {
    delete resource;
  }
  return HEAP->undefined_value();
}


class LolPathTracer: public PathTracer {
 public:
  LolPathTracer(FILE* out,
                Object* search_target,
                WhatToFind what_to_find)
      : PathTracer(search_target, what_to_find, VISIT_ONLY_STRONG), out_(out) {}

 private:
  void ProcessResults();

  FILE* out_;
};


void LolPathTracer::ProcessResults() {
  if (found_target_) {
    PrintF(out_, "=====================================\n");
    PrintF(out_, "====        Path to object       ====\n");
    PrintF(out_, "=====================================\n\n");

    ASSERT(!object_stack_.is_empty());
    Object* prev = NULL;
    for (int i = 0, index = 0; i < object_stack_.length(); i++) {
      Object* obj = object_stack_[i];

      // Skip this object if it is basically the internals of the
      // previous object (which would have dumped its details already).
      if (prev && prev->IsJSObject() &&
          (obj != search_target_)) {
        JSObject* jsobj = JSObject::cast(prev);
        if (obj->IsFixedArray() &&
            jsobj->properties() == FixedArray::cast(obj)) {
          // Skip this one because it would have been printed as the
          // properties of the last object already.
          continue;
        } else if (obj->IsHeapObject() &&
                   jsobj->elements() == HeapObject::cast(obj)) {
          // Skip this one because it would have been printed as the
          // elements of the last object already.
          continue;
        }
      }

      // Print a connecting arrow.
      if (i > 0) PrintF(out_, "\n     |\n     |\n     V\n\n");

      // Print the object index.
      PrintF(out_, "[%d] ", ++index);

      // Print the LOL object ID:
      int id = LiveObjectList::GetObjId(obj);
      if (id > 0) PrintF(out_, "@%d ", id);

#ifdef OBJECT_PRINT
#ifdef INSPECTOR
      Inspector::DumpObjectType(out_, obj);
#endif  // INSPECTOR
      PrintF(out_, "\n");
      obj->Print(out_);
#else  // !OBJECT_PRINT
      obj->ShortPrint(out_);
      PrintF(out_, "\n");
#endif  // !OBJECT_PRINT
      Flush(out_);
    }
    PrintF(out_, "\n");
    PrintF(out_, "=====================================\n\n");
    Flush(out_);
  }
}


Object* LiveObjectList::GetPathPrivate(HeapObject* obj1, HeapObject* obj2) {
  EmbeddedVector<char, 128> temp_filename;
  static int temp_count = 0;
  const char* path_prefix = ".";

  if (FLAG_lol_workdir) {
    path_prefix = FLAG_lol_workdir;
  }
  OS::SNPrintF(temp_filename, "%s/lol-getpath-%d", path_prefix, ++temp_count);

  FILE* f = OS::FOpen(temp_filename.start(), "w+");

  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  // Save the previous verbosity.
  bool prev_verbosity = FLAG_use_verbose_printer;
  FLAG_use_verbose_printer = false;

  // Dump the paths.
  {
    // The tracer needs to be scoped because its usage asserts no allocation,
    // and we need to allocate the result string below.
    LolPathTracer tracer(f, obj2, LolPathTracer::FIND_FIRST);

    bool found = false;
    if (obj1 == NULL) {
      // Check for ObjectGroups that references this object.
      // TODO(mlam): refactor this to be more modular.
      {
        List<ObjectGroup*>* groups = isolate->global_handles()->object_groups();
        for (int i = 0; i < groups->length(); i++) {
          ObjectGroup* group = groups->at(i);
          if (group == NULL) continue;

          bool found_group = false;
          for (size_t j = 0; j < group->length_; j++) {
            Object* object = *(group->objects_[j]);
            HeapObject* hobj = HeapObject::cast(object);
            if (obj2 == hobj) {
              found_group = true;
              break;
            }
          }

          if (found_group) {
            PrintF(f,
                   "obj %p is a member of object group %p {\n",
                   reinterpret_cast<void*>(obj2),
                   reinterpret_cast<void*>(group));
            for (size_t j = 0; j < group->length_; j++) {
              Object* object = *(group->objects_[j]);
              if (!object->IsHeapObject()) continue;

              HeapObject* hobj = HeapObject::cast(object);
              int id = GetObjId(hobj);
              if (id != 0) {
                PrintF(f, "  @%d:", id);
              } else {
                PrintF(f, "  <no id>:");
              }

              char buffer[512];
              GenerateObjectDesc(hobj, buffer, sizeof(buffer));
              PrintF(f, " %s", buffer);
              if (hobj == obj2) {
                PrintF(f, " <===");
              }
              PrintF(f, "\n");
            }
            PrintF(f, "}\n");
          }
        }
      }

      PrintF(f, "path from roots to obj %p\n", reinterpret_cast<void*>(obj2));
      heap->IterateRoots(&tracer, VISIT_ONLY_STRONG);
      found = tracer.found();

      if (!found) {
        PrintF(f, "  No paths found. Checking symbol tables ...\n");
        SymbolTable* symbol_table = HEAP->raw_unchecked_symbol_table();
        tracer.VisitPointers(reinterpret_cast<Object**>(&symbol_table),
                             reinterpret_cast<Object**>(&symbol_table)+1);
        found = tracer.found();
        if (!found) {
          symbol_table->IteratePrefix(&tracer);
          found = tracer.found();
        }
      }

      if (!found) {
        PrintF(f, "  No paths found. Checking weak roots ...\n");
        // Check weak refs next.
        isolate->global_handles()->IterateWeakRoots(&tracer);
        found = tracer.found();
      }

    } else {
      PrintF(f, "path from obj %p to obj %p:\n",
             reinterpret_cast<void*>(obj1), reinterpret_cast<void*>(obj2));
      tracer.TracePathFrom(reinterpret_cast<Object**>(&obj1));
      found = tracer.found();
    }

    if (!found) {
      PrintF(f, "  No paths found\n\n");
    }
  }

  // Flush and clean up the dumped file.
  Flush(f);
  fclose(f);

  // Restore the previous verbosity.
  FLAG_use_verbose_printer = prev_verbosity;

  // Create a string from the temp_file.
  // Note: the mmapped resource will take care of closing the file.
  MemoryMappedExternalResource* resource =
      new MemoryMappedExternalResource(temp_filename.start(), true);
  if (resource->exists() && !resource->is_empty()) {
    ASSERT(resource->IsAscii());
    Handle<String> path_string =
        factory->NewExternalStringFromAscii(resource);
    heap->external_string_table()->AddString(*path_string);
    return *path_string;
  } else {
    delete resource;
  }
  return heap->undefined_value();
}


Object* LiveObjectList::GetPath(int obj_id1,
                                int obj_id2,
                                Handle<JSObject> instance_filter) {
  HandleScope scope;

  // Get the target object.
  HeapObject* obj1 = NULL;
  if (obj_id1 != 0) {
    obj1 = HeapObject::cast(GetObj(obj_id1));
    if (obj1 == HEAP->undefined_value()) {
      return obj1;
    }
  }

  HeapObject* obj2 = HeapObject::cast(GetObj(obj_id2));
  if (obj2 == HEAP->undefined_value()) {
    return obj2;
  }

  return GetPathPrivate(obj1, obj2);
}


void LiveObjectList::DoProcessNonLive(HeapObject* obj) {
  // We should only be called if we have at least one lol to search.
  ASSERT(last() != NULL);
  Element* element = last()->Find(obj);
  if (element != NULL) {
    NullifyNonLivePointer(&element->obj_);
  }
}


void LiveObjectList::IterateElementsPrivate(ObjectVisitor* v) {
  LiveObjectList* lol = last();
  while (lol != NULL) {
    Element* elements = lol->elements_;
    int count = lol->obj_count_;
    for (int i = 0; i < count; i++) {
      HeapObject** p = &elements[i].obj_;
      v->VisitPointer(reinterpret_cast<Object** >(p));
    }
    lol = lol->prev_;
  }
}


// Purpose: Called by GCEpilogue to purge duplicates.  Not to be called by
// anyone else.
void LiveObjectList::PurgeDuplicates() {
  bool is_sorted = false;
  LiveObjectList* lol = last();
  if (!lol) {
    return;  // Nothing to purge.
  }

  int total_count = lol->TotalObjCount();
  if (!total_count) {
    return;  // Nothing to purge.
  }

  Element* elements = NewArray<Element>(total_count);
  int count = 0;

  // Copy all the object elements into a consecutive array.
  while (lol) {
    memcpy(&elements[count], lol->elements_, lol->obj_count_ * sizeof(Element));
    count += lol->obj_count_;
    lol = lol->prev_;
  }
  qsort(elements, total_count, sizeof(Element),
        reinterpret_cast<RawComparer>(CompareElement));

  ASSERT(count == total_count);

  // Iterate over all objects in the consolidated list and check for dups.
  total_count--;
  for (int i = 0; i < total_count; ) {
    Element* curr = &elements[i];
    HeapObject* curr_obj = curr->obj_;
    int j = i+1;
    bool done = false;

    while (!done && (j < total_count)) {
      // Process if the element's object is still live after the current GC.
      // Non-live objects will be converted to SMIs i.e. not HeapObjects.
      if (curr_obj->IsHeapObject()) {
        Element* next = &elements[j];
        HeapObject* next_obj = next->obj_;
        if (next_obj->IsHeapObject()) {
          if (curr_obj != next_obj) {
            done = true;
            continue;  // Live object but no match.  Move on.
          }

          // NOTE: we've just GCed the LOLs.  Hence, they are no longer sorted.
          // Since we detected at least one need to search for entries, we'll
          // sort it to enable the use of NullifyMostRecent() below.  We only
          // need to sort it once (except for one exception ... see below).
          if (!is_sorted) {
            SortAll();
            is_sorted = true;
          }

          // We have a match.  Need to nullify the most recent ref to this
          // object.  We'll keep the oldest ref:
          // Note: we will nullify the element record in the LOL
          // database, not in the local sorted copy of the elements.
          NullifyMostRecent(curr_obj);
        }
      }
      // Either the object was already marked for purging, or we just marked
      // it.  Either way, if there's more than one dup, then we need to check
      // the next element for another possible dup against the current as well
      // before we move on.  So, here we go.
      j++;
    }

    // We can move on to checking the match on the next element.
    i = j;
  }

  DeleteArray<Element>(elements);
}


// Purpose: Purges dead objects and resorts the LOLs.
void LiveObjectList::GCEpiloguePrivate() {
  // Note: During the GC, ConsStrings may be collected and pointers may be
  // forwarded to its constituent string.  As a result, we may find dupes of
  // objects references in the LOL list.
  // Another common way we get dups is that free chunks that have been swept
  // in the oldGen heap may be kept as ByteArray objects in a free list.
  //
  // When we promote live objects from the youngGen, the object may be moved
  // to the start of these free chunks.  Since there is no free or move event
  // for the free chunks, their addresses will show up 2 times: once for their
  // original free ByteArray selves, and once for the newly promoted youngGen
  // object.  Hence, we can get a duplicate address in the LOL again.
  //
  // We need to eliminate these dups because the LOL implementation expects to
  // only have at most one unique LOL reference to any object at any time.
  PurgeDuplicates();

  // After the GC, sweep away all free'd Elements and compact.
  LiveObjectList* prev = NULL;
  LiveObjectList* next = NULL;

  // Iterating from the youngest lol to the oldest lol.
  for (LiveObjectList* lol = last(); lol; lol = prev) {
    Element* elements = lol->elements_;
    prev = lol->prev();  // Save the prev.

    // Remove any references to collected objects.
    int i = 0;
    while (i < lol->obj_count_) {
      Element& element = elements[i];
      if (!element.obj_->IsHeapObject()) {
        // If the HeapObject address was converted into a SMI, then this
        // is a dead object.  Copy the last element over this one.
        element = elements[lol->obj_count_ - 1];
        lol->obj_count_--;
        // We've just moved the last element into this index.  We'll revisit
        // this index again.  Hence, no need to increment the iterator.
      } else {
        i++;  // Look at the next element next.
      }
    }

    int new_count = lol->obj_count_;

    // Check if there are any more elements to keep after purging the dead ones.
    if (new_count == 0) {
      DeleteArray<Element>(elements);
      lol->elements_ = NULL;
      lol->capacity_ = 0;
      ASSERT(lol->obj_count_ == 0);

      // If the list is also invisible, the clean up the list as well.
      if (lol->id_ == 0) {
        // Point the next lol's prev to this lol's prev.
        if (next) {
          next->prev_ = lol->prev_;
        } else {
          last_ = lol->prev_;
        }

        // Delete this now empty and invisible lol.
        delete lol;

        // Don't point the next to this lol since it is now deleted.
        // Leave the next pointer pointing to the current lol.
        continue;
      }

    } else {
      // If the obj_count_ is less than the capacity and the difference is
      // greater than a specified threshold, then we should shrink the list.
      int diff = lol->capacity_ - new_count;
      const int kMaxUnusedSpace = 64;
      if (diff > kMaxUnusedSpace) {  // Threshold for shrinking.
        // Shrink the list.
        Element* new_elements = NewArray<Element>(new_count);
        memcpy(new_elements, elements, new_count * sizeof(Element));

        DeleteArray<Element>(elements);
        lol->elements_ = new_elements;
        lol->capacity_ = new_count;
      }
      ASSERT(lol->obj_count_ == new_count);

      lol->Sort();  // We've moved objects.  Re-sort in case.
    }

    // Save the next (for the previous link) in case we need it later.
    next = lol;
  }

#ifdef VERIFY_LOL
  if (FLAG_verify_lol) {
    Verify();
  }
#endif
}


#ifdef VERIFY_LOL
void LiveObjectList::Verify(bool match_heap_exactly) {
  OS::Print("Verifying the LiveObjectList database:\n");

  LiveObjectList* lol = last();
  if (lol == NULL) {
    OS::Print("  No lol database to verify\n");
    return;
  }

  OS::Print("  Preparing the lol database ...\n");
  int total_count = lol->TotalObjCount();

  Element* elements = NewArray<Element>(total_count);
  int count = 0;

  // Copy all the object elements into a consecutive array.
  OS::Print("  Copying the lol database ...\n");
  while (lol != NULL) {
    memcpy(&elements[count], lol->elements_, lol->obj_count_ * sizeof(Element));
    count += lol->obj_count_;
    lol = lol->prev_;
  }
  qsort(elements, total_count, sizeof(Element),
        reinterpret_cast<RawComparer>(CompareElement));

  ASSERT(count == total_count);

  // Iterate over all objects in the heap and check for:
  // 1. object in LOL but not in heap i.e. error.
  // 2. object in heap but not in LOL (possibly not an error).  Usually
  //    just means that we don't have the a capture of the latest heap.
  //    That is unless we did this verify immediately after a capture,
  //    and specified match_heap_exactly = true.

  int number_of_heap_objects = 0;
  int number_of_matches = 0;
  int number_not_in_heap = total_count;
  int number_not_in_lol = 0;

  OS::Print("  Start verify ...\n");
  OS::Print("  Verifying ...");
  Flush();
  HeapIterator iterator;
  HeapObject* heap_obj = NULL;
  while ((heap_obj = iterator.next()) != NULL) {
    number_of_heap_objects++;

    // Check if the heap_obj is in the lol.
    Element key;
    key.obj_ = heap_obj;

    Element* result = reinterpret_cast<Element*>(
        bsearch(&key, elements, total_count, sizeof(Element),
                reinterpret_cast<RawComparer>(CompareElement)));

    if (result != NULL) {
      number_of_matches++;
      number_not_in_heap--;
      // Mark it as found by changing it into a SMI (mask off low bit).
      // Note: we cannot use HeapObject::cast() here because it asserts that
      // the HeapObject bit is set on the address, but we're unsetting it on
      // purpose here for our marking.
      result->obj_ = reinterpret_cast<HeapObject*>(heap_obj->address());

    } else {
      number_not_in_lol++;
      if (match_heap_exactly) {
        OS::Print("heap object %p NOT in lol database\n", heap_obj);
      }
    }
    // Show some sign of life.
    if (number_of_heap_objects % 1000 == 0) {
      OS::Print(".");
      fflush(stdout);
    }
  }
  OS::Print("\n");

  // Reporting lol objects not found in the heap.
  if (number_not_in_heap) {
    int found = 0;
    for (int i = 0; (i < total_count) && (found < number_not_in_heap); i++) {
      Element& element = elements[i];
      if (element.obj_->IsHeapObject()) {
        OS::Print("lol database object [%d of %d] %p NOT in heap\n",
                  i, total_count, element.obj_);
        found++;
      }
    }
  }

  DeleteArray<Element>(elements);

  OS::Print("number of objects in lol database %d\n", total_count);
  OS::Print("number of heap objects .......... %d\n", number_of_heap_objects);
  OS::Print("number of matches ............... %d\n", number_of_matches);
  OS::Print("number NOT in heap .............. %d\n", number_not_in_heap);
  OS::Print("number NOT in lol database ...... %d\n", number_not_in_lol);

  if (number_of_matches != total_count) {
    OS::Print("  *** ERROR: "
              "NOT all lol database objects match heap objects.\n");
  }
  if (number_not_in_heap != 0) {
    OS::Print("  *** ERROR: %d lol database objects not found in heap.\n",
              number_not_in_heap);
  }
  if (match_heap_exactly) {
    if (!(number_not_in_lol == 0)) {
      OS::Print("  *** ERROR: %d heap objects NOT found in lol database.\n",
                number_not_in_lol);
    }
  }

  ASSERT(number_of_matches == total_count);
  ASSERT(number_not_in_heap == 0);
  ASSERT(number_not_in_lol == (number_of_heap_objects - total_count));
  if (match_heap_exactly) {
    ASSERT(total_count == number_of_heap_objects);
    ASSERT(number_not_in_lol == 0);
  }

  OS::Print("  Verify the lol database is sorted ...\n");
  lol = last();
  while (lol != NULL) {
    Element* elements = lol->elements_;
    for (int i = 0; i < lol->obj_count_ - 1; i++) {
      if (elements[i].obj_ >= elements[i+1].obj_) {
        OS::Print("  *** ERROR: lol %p obj[%d] %p > obj[%d] %p\n",
                  lol, i, elements[i].obj_, i+1, elements[i+1].obj_);
      }
    }
    lol = lol->prev_;
  }

  OS::Print("  DONE verifying.\n\n\n");
}


void LiveObjectList::VerifyNotInFromSpace() {
  OS::Print("VerifyNotInFromSpace() ...\n");
  LolIterator it(NULL, last());
  Heap* heap = ISOLATE->heap();
  int i = 0;
  for (it.Init(); !it.Done(); it.Next()) {
    HeapObject* heap_obj = it.Obj();
    if (heap->InFromSpace(heap_obj)) {
      OS::Print(" ERROR: VerifyNotInFromSpace: [%d] obj %p in From space %p\n",
                i++, heap_obj, Heap::new_space()->FromSpaceStart());
    }
  }
}
#endif  // VERIFY_LOL


} }  // namespace v8::internal

#endif  // LIVE_OBJECT_LIST
