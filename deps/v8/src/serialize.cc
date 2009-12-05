// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "accessors.h"
#include "api.h"
#include "execution.h"
#include "global-handles.h"
#include "ic-inl.h"
#include "natives.h"
#include "platform.h"
#include "runtime.h"
#include "serialize.h"
#include "stub-cache.h"
#include "v8threads.h"
#include "top.h"
#include "bootstrapper.h"

namespace v8 {
namespace internal {

// Mapping objects to their location after deserialization.
// This is used during building, but not at runtime by V8.
class SerializationAddressMapper {
 public:
  static bool IsMapped(HeapObject* obj) {
    EnsureMapExists();
    return serialization_map_->Lookup(Key(obj), Hash(obj), false) != NULL;
  }

  static int MappedTo(HeapObject* obj) {
    ASSERT(IsMapped(obj));
    return reinterpret_cast<intptr_t>(serialization_map_->Lookup(Key(obj),
                                      Hash(obj),
                                      false)->value);
  }

  static void Map(HeapObject* obj, int to) {
    EnsureMapExists();
    ASSERT(!IsMapped(obj));
    HashMap::Entry* entry =
        serialization_map_->Lookup(Key(obj), Hash(obj), true);
    entry->value = Value(to);
  }

  static void Zap() {
    if (serialization_map_ != NULL) {
      delete serialization_map_;
    }
    serialization_map_ = NULL;
  }

 private:
  static bool SerializationMatchFun(void* key1, void* key2) {
    return key1 == key2;
  }

  static uint32_t Hash(HeapObject* obj) {
    return reinterpret_cast<intptr_t>(obj->address());
  }

  static void* Key(HeapObject* obj) {
    return reinterpret_cast<void*>(obj->address());
  }

  static void* Value(int v) {
    return reinterpret_cast<void*>(v);
  }

  static void EnsureMapExists() {
    if (serialization_map_ == NULL) {
      serialization_map_ = new HashMap(&SerializationMatchFun);
    }
  }

  static HashMap* serialization_map_;
};


HashMap* SerializationAddressMapper::serialization_map_ = NULL;




// -----------------------------------------------------------------------------
// Coding of external references.

// The encoding of an external reference. The type is in the high word.
// The id is in the low word.
static uint32_t EncodeExternal(TypeCode type, uint16_t id) {
  return static_cast<uint32_t>(type) << 16 | id;
}


static int* GetInternalPointer(StatsCounter* counter) {
  // All counters refer to dummy_counter, if deserializing happens without
  // setting up counters.
  static int dummy_counter = 0;
  return counter->Enabled() ? counter->GetInternalPointer() : &dummy_counter;
}


// ExternalReferenceTable is a helper class that defines the relationship
// between external references and their encodings. It is used to build
// hashmaps in ExternalReferenceEncoder and ExternalReferenceDecoder.
class ExternalReferenceTable {
 public:
  static ExternalReferenceTable* instance() {
    if (!instance_) instance_ = new ExternalReferenceTable();
    return instance_;
  }

  int size() const { return refs_.length(); }

  Address address(int i) { return refs_[i].address; }

  uint32_t code(int i) { return refs_[i].code; }

  const char* name(int i) { return refs_[i].name; }

  int max_id(int code) { return max_id_[code]; }

 private:
  static ExternalReferenceTable* instance_;

  ExternalReferenceTable() : refs_(64) { PopulateTable(); }
  ~ExternalReferenceTable() { }

  struct ExternalReferenceEntry {
    Address address;
    uint32_t code;
    const char* name;
  };

  void PopulateTable();

  // For a few types of references, we can get their address from their id.
  void AddFromId(TypeCode type, uint16_t id, const char* name);

  // For other types of references, the caller will figure out the address.
  void Add(Address address, TypeCode type, uint16_t id, const char* name);

  List<ExternalReferenceEntry> refs_;
  int max_id_[kTypeCodeCount];
};


ExternalReferenceTable* ExternalReferenceTable::instance_ = NULL;


void ExternalReferenceTable::AddFromId(TypeCode type,
                                       uint16_t id,
                                       const char* name) {
  Address address;
  switch (type) {
    case C_BUILTIN: {
      ExternalReference ref(static_cast<Builtins::CFunctionId>(id));
      address = ref.address();
      break;
    }
    case BUILTIN: {
      ExternalReference ref(static_cast<Builtins::Name>(id));
      address = ref.address();
      break;
    }
    case RUNTIME_FUNCTION: {
      ExternalReference ref(static_cast<Runtime::FunctionId>(id));
      address = ref.address();
      break;
    }
    case IC_UTILITY: {
      ExternalReference ref(IC_Utility(static_cast<IC::UtilityId>(id)));
      address = ref.address();
      break;
    }
    default:
      UNREACHABLE();
      return;
  }
  Add(address, type, id, name);
}


void ExternalReferenceTable::Add(Address address,
                                 TypeCode type,
                                 uint16_t id,
                                 const char* name) {
  ASSERT_NE(NULL, address);
  ExternalReferenceEntry entry;
  entry.address = address;
  entry.code = EncodeExternal(type, id);
  entry.name = name;
  ASSERT_NE(0, entry.code);
  refs_.Add(entry);
  if (id > max_id_[type]) max_id_[type] = id;
}


void ExternalReferenceTable::PopulateTable() {
  for (int type_code = 0; type_code < kTypeCodeCount; type_code++) {
    max_id_[type_code] = 0;
  }

  // The following populates all of the different type of external references
  // into the ExternalReferenceTable.
  //
  // NOTE: This function was originally 100k of code.  It has since been
  // rewritten to be mostly table driven, as the callback macro style tends to
  // very easily cause code bloat.  Please be careful in the future when adding
  // new references.

  struct RefTableEntry {
    TypeCode type;
    uint16_t id;
    const char* name;
  };

  static const RefTableEntry ref_table[] = {
  // Builtins
#define DEF_ENTRY_C(name) \
  { C_BUILTIN, \
    Builtins::c_##name, \
    "Builtins::" #name },

  BUILTIN_LIST_C(DEF_ENTRY_C)
#undef DEF_ENTRY_C

#define DEF_ENTRY_C(name) \
  { BUILTIN, \
    Builtins::name, \
    "Builtins::" #name },
#define DEF_ENTRY_A(name, kind, state) DEF_ENTRY_C(name)

  BUILTIN_LIST_C(DEF_ENTRY_C)
  BUILTIN_LIST_A(DEF_ENTRY_A)
  BUILTIN_LIST_DEBUG_A(DEF_ENTRY_A)
#undef DEF_ENTRY_C
#undef DEF_ENTRY_A

  // Runtime functions
#define RUNTIME_ENTRY(name, nargs, ressize) \
  { RUNTIME_FUNCTION, \
    Runtime::k##name, \
    "Runtime::" #name },

  RUNTIME_FUNCTION_LIST(RUNTIME_ENTRY)
#undef RUNTIME_ENTRY

  // IC utilities
#define IC_ENTRY(name) \
  { IC_UTILITY, \
    IC::k##name, \
    "IC::" #name },

  IC_UTIL_LIST(IC_ENTRY)
#undef IC_ENTRY
  };  // end of ref_table[].

  for (size_t i = 0; i < ARRAY_SIZE(ref_table); ++i) {
    AddFromId(ref_table[i].type, ref_table[i].id, ref_table[i].name);
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Debug addresses
  Add(Debug_Address(Debug::k_after_break_target_address).address(),
      DEBUG_ADDRESS,
      Debug::k_after_break_target_address << kDebugIdShift,
      "Debug::after_break_target_address()");
  Add(Debug_Address(Debug::k_debug_break_return_address).address(),
      DEBUG_ADDRESS,
      Debug::k_debug_break_return_address << kDebugIdShift,
      "Debug::debug_break_return_address()");
  const char* debug_register_format = "Debug::register_address(%i)";
  int dr_format_length = StrLength(debug_register_format);
  for (int i = 0; i < kNumJSCallerSaved; ++i) {
    Vector<char> name = Vector<char>::New(dr_format_length + 1);
    OS::SNPrintF(name, debug_register_format, i);
    Add(Debug_Address(Debug::k_register_address, i).address(),
        DEBUG_ADDRESS,
        Debug::k_register_address << kDebugIdShift | i,
        name.start());
  }
#endif

  // Stat counters
  struct StatsRefTableEntry {
    StatsCounter* counter;
    uint16_t id;
    const char* name;
  };

  static const StatsRefTableEntry stats_ref_table[] = {
#define COUNTER_ENTRY(name, caption) \
  { &Counters::name, \
    Counters::k_##name, \
    "Counters::" #name },

  STATS_COUNTER_LIST_1(COUNTER_ENTRY)
  STATS_COUNTER_LIST_2(COUNTER_ENTRY)
#undef COUNTER_ENTRY
  };  // end of stats_ref_table[].

  for (size_t i = 0; i < ARRAY_SIZE(stats_ref_table); ++i) {
    Add(reinterpret_cast<Address>(
            GetInternalPointer(stats_ref_table[i].counter)),
        STATS_COUNTER,
        stats_ref_table[i].id,
        stats_ref_table[i].name);
  }

  // Top addresses
  const char* top_address_format = "Top::%s";

  const char* AddressNames[] = {
#define C(name) #name,
    TOP_ADDRESS_LIST(C)
    TOP_ADDRESS_LIST_PROF(C)
    NULL
#undef C
  };

  int top_format_length = StrLength(top_address_format) - 2;
  for (uint16_t i = 0; i < Top::k_top_address_count; ++i) {
    const char* address_name = AddressNames[i];
    Vector<char> name =
        Vector<char>::New(top_format_length + StrLength(address_name) + 1);
    const char* chars = name.start();
    OS::SNPrintF(name, top_address_format, address_name);
    Add(Top::get_address_from_id((Top::AddressId)i), TOP_ADDRESS, i, chars);
  }

  // Extensions
  Add(FUNCTION_ADDR(GCExtension::GC), EXTENSION, 1,
      "GCExtension::GC");

  // Accessors
#define ACCESSOR_DESCRIPTOR_DECLARATION(name) \
  Add((Address)&Accessors::name, \
      ACCESSOR, \
      Accessors::k##name, \
      "Accessors::" #name);

  ACCESSOR_DESCRIPTOR_LIST(ACCESSOR_DESCRIPTOR_DECLARATION)
#undef ACCESSOR_DESCRIPTOR_DECLARATION

  // Stub cache tables
  Add(SCTableReference::keyReference(StubCache::kPrimary).address(),
      STUB_CACHE_TABLE,
      1,
      "StubCache::primary_->key");
  Add(SCTableReference::valueReference(StubCache::kPrimary).address(),
      STUB_CACHE_TABLE,
      2,
      "StubCache::primary_->value");
  Add(SCTableReference::keyReference(StubCache::kSecondary).address(),
      STUB_CACHE_TABLE,
      3,
      "StubCache::secondary_->key");
  Add(SCTableReference::valueReference(StubCache::kSecondary).address(),
      STUB_CACHE_TABLE,
      4,
      "StubCache::secondary_->value");

  // Runtime entries
  Add(ExternalReference::perform_gc_function().address(),
      RUNTIME_ENTRY,
      1,
      "Runtime::PerformGC");
  Add(ExternalReference::random_positive_smi_function().address(),
      RUNTIME_ENTRY,
      2,
      "V8::RandomPositiveSmi");

  // Miscellaneous
  Add(ExternalReference::builtin_passed_function().address(),
      UNCLASSIFIED,
      1,
      "Builtins::builtin_passed_function");
  Add(ExternalReference::the_hole_value_location().address(),
      UNCLASSIFIED,
      2,
      "Factory::the_hole_value().location()");
  Add(ExternalReference::roots_address().address(),
      UNCLASSIFIED,
      3,
      "Heap::roots_address()");
  Add(ExternalReference::address_of_stack_limit().address(),
      UNCLASSIFIED,
      4,
      "StackGuard::address_of_jslimit()");
  Add(ExternalReference::address_of_real_stack_limit().address(),
      UNCLASSIFIED,
      5,
      "StackGuard::address_of_real_jslimit()");
  Add(ExternalReference::address_of_regexp_stack_limit().address(),
      UNCLASSIFIED,
      6,
      "RegExpStack::limit_address()");
  Add(ExternalReference::new_space_start().address(),
      UNCLASSIFIED,
      7,
      "Heap::NewSpaceStart()");
  Add(ExternalReference::heap_always_allocate_scope_depth().address(),
      UNCLASSIFIED,
      8,
      "Heap::always_allocate_scope_depth()");
  Add(ExternalReference::new_space_allocation_limit_address().address(),
      UNCLASSIFIED,
      9,
      "Heap::NewSpaceAllocationLimitAddress()");
  Add(ExternalReference::new_space_allocation_top_address().address(),
      UNCLASSIFIED,
      10,
      "Heap::NewSpaceAllocationTopAddress()");
#ifdef ENABLE_DEBUGGER_SUPPORT
  Add(ExternalReference::debug_break().address(),
      UNCLASSIFIED,
      11,
      "Debug::Break()");
  Add(ExternalReference::debug_step_in_fp_address().address(),
      UNCLASSIFIED,
      12,
      "Debug::step_in_fp_addr()");
#endif
  Add(ExternalReference::double_fp_operation(Token::ADD).address(),
      UNCLASSIFIED,
      13,
      "add_two_doubles");
  Add(ExternalReference::double_fp_operation(Token::SUB).address(),
      UNCLASSIFIED,
      14,
      "sub_two_doubles");
  Add(ExternalReference::double_fp_operation(Token::MUL).address(),
      UNCLASSIFIED,
      15,
      "mul_two_doubles");
  Add(ExternalReference::double_fp_operation(Token::DIV).address(),
      UNCLASSIFIED,
      16,
      "div_two_doubles");
  Add(ExternalReference::double_fp_operation(Token::MOD).address(),
      UNCLASSIFIED,
      17,
      "mod_two_doubles");
  Add(ExternalReference::compare_doubles().address(),
      UNCLASSIFIED,
      18,
      "compare_doubles");
#ifdef V8_NATIVE_REGEXP
  Add(ExternalReference::re_case_insensitive_compare_uc16().address(),
      UNCLASSIFIED,
      19,
      "NativeRegExpMacroAssembler::CaseInsensitiveCompareUC16()");
  Add(ExternalReference::re_check_stack_guard_state().address(),
      UNCLASSIFIED,
      20,
      "RegExpMacroAssembler*::CheckStackGuardState()");
  Add(ExternalReference::re_grow_stack().address(),
      UNCLASSIFIED,
      21,
      "NativeRegExpMacroAssembler::GrowStack()");
#endif
}


ExternalReferenceEncoder::ExternalReferenceEncoder()
    : encodings_(Match) {
  ExternalReferenceTable* external_references =
      ExternalReferenceTable::instance();
  for (int i = 0; i < external_references->size(); ++i) {
    Put(external_references->address(i), i);
  }
}


uint32_t ExternalReferenceEncoder::Encode(Address key) const {
  int index = IndexOf(key);
  return index >=0 ? ExternalReferenceTable::instance()->code(index) : 0;
}


const char* ExternalReferenceEncoder::NameOfAddress(Address key) const {
  int index = IndexOf(key);
  return index >=0 ? ExternalReferenceTable::instance()->name(index) : NULL;
}


int ExternalReferenceEncoder::IndexOf(Address key) const {
  if (key == NULL) return -1;
  HashMap::Entry* entry =
      const_cast<HashMap &>(encodings_).Lookup(key, Hash(key), false);
  return entry == NULL
      ? -1
      : static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
}


void ExternalReferenceEncoder::Put(Address key, int index) {
  HashMap::Entry* entry = encodings_.Lookup(key, Hash(key), true);
  entry->value = reinterpret_cast<void *>(index);
}


ExternalReferenceDecoder::ExternalReferenceDecoder()
  : encodings_(NewArray<Address*>(kTypeCodeCount)) {
  ExternalReferenceTable* external_references =
      ExternalReferenceTable::instance();
  for (int type = kFirstTypeCode; type < kTypeCodeCount; ++type) {
    int max = external_references->max_id(type) + 1;
    encodings_[type] = NewArray<Address>(max + 1);
  }
  for (int i = 0; i < external_references->size(); ++i) {
    Put(external_references->code(i), external_references->address(i));
  }
}


ExternalReferenceDecoder::~ExternalReferenceDecoder() {
  for (int type = kFirstTypeCode; type < kTypeCodeCount; ++type) {
    DeleteArray(encodings_[type]);
  }
  DeleteArray(encodings_);
}


bool Serializer::serialization_enabled_ = false;
bool Serializer::too_late_to_enable_now_ = false;


Deserializer::Deserializer(SnapshotByteSource* source)
    : source_(source),
      external_reference_decoder_(NULL) {
}


// This routine both allocates a new object, and also keeps
// track of where objects have been allocated so that we can
// fix back references when deserializing.
Address Deserializer::Allocate(int space_index, Space* space, int size) {
  Address address;
  if (!SpaceIsLarge(space_index)) {
    ASSERT(!SpaceIsPaged(space_index) ||
           size <= Page::kPageSize - Page::kObjectStartOffset);
    Object* new_allocation;
    if (space_index == NEW_SPACE) {
      new_allocation = reinterpret_cast<NewSpace*>(space)->AllocateRaw(size);
    } else {
      new_allocation = reinterpret_cast<PagedSpace*>(space)->AllocateRaw(size);
    }
    HeapObject* new_object = HeapObject::cast(new_allocation);
    ASSERT(!new_object->IsFailure());
    address = new_object->address();
    high_water_[space_index] = address + size;
  } else {
    ASSERT(SpaceIsLarge(space_index));
    ASSERT(size > Page::kPageSize - Page::kObjectStartOffset);
    LargeObjectSpace* lo_space = reinterpret_cast<LargeObjectSpace*>(space);
    Object* new_allocation;
    if (space_index == kLargeData) {
      new_allocation = lo_space->AllocateRaw(size);
    } else if (space_index == kLargeFixedArray) {
      new_allocation = lo_space->AllocateRawFixedArray(size);
    } else {
      ASSERT_EQ(kLargeCode, space_index);
      new_allocation = lo_space->AllocateRawCode(size);
    }
    ASSERT(!new_allocation->IsFailure());
    HeapObject* new_object = HeapObject::cast(new_allocation);
    // Record all large objects in the same space.
    address = new_object->address();
    high_water_[LO_SPACE] = address + size;
  }
  last_object_address_ = address;
  return address;
}


// This returns the address of an object that has been described in the
// snapshot as being offset bytes back in a particular space.
HeapObject* Deserializer::GetAddressFromEnd(int space) {
  int offset = source_->GetInt();
  ASSERT(!SpaceIsLarge(space));
  offset <<= kObjectAlignmentBits;
  return HeapObject::FromAddress(high_water_[space] - offset);
}


// This returns the address of an object that has been described in the
// snapshot as being offset bytes into a particular space.
HeapObject* Deserializer::GetAddressFromStart(int space) {
  int offset = source_->GetInt();
  if (SpaceIsLarge(space)) {
    // Large spaces have one object per 'page'.
    return HeapObject::FromAddress(pages_[LO_SPACE][offset]);
  }
  offset <<= kObjectAlignmentBits;
  if (space == NEW_SPACE) {
    // New space has only one space - numbered 0.
    return HeapObject::FromAddress(pages_[space][0] + offset);
  }
  ASSERT(SpaceIsPaged(space));
  int page_of_pointee = offset >> Page::kPageSizeBits;
  Address object_address = pages_[space][page_of_pointee] +
                           (offset & Page::kPageAlignmentMask);
  return HeapObject::FromAddress(object_address);
}


void Deserializer::Deserialize() {
  // Don't GC while deserializing - just expand the heap.
  AlwaysAllocateScope always_allocate;
  // Don't use the free lists while deserializing.
  LinearAllocationScope allocate_linearly;
  // No active threads.
  ASSERT_EQ(NULL, ThreadState::FirstInUse());
  // No active handles.
  ASSERT(HandleScopeImplementer::instance()->blocks()->is_empty());
  ASSERT_EQ(NULL, external_reference_decoder_);
  external_reference_decoder_ = new ExternalReferenceDecoder();
  Heap::IterateRoots(this, VISIT_ONLY_STRONG);
  ASSERT(source_->AtEOF());
  delete external_reference_decoder_;
  external_reference_decoder_ = NULL;
}


// This is called on the roots.  It is the driver of the deserialization
// process.  It is also called on the body of each function.
void Deserializer::VisitPointers(Object** start, Object** end) {
  // The space must be new space.  Any other space would cause ReadChunk to try
  // to update the remembered using NULL as the address.
  ReadChunk(start, end, NEW_SPACE, NULL);
}


// This routine writes the new object into the pointer provided and then
// returns true if the new object was in young space and false otherwise.
// The reason for this strange interface is that otherwise the object is
// written very late, which means the ByteArray map is not set up by the
// time we need to use it to mark the space at the end of a page free (by
// making it into a byte array).
void Deserializer::ReadObject(int space_number,
                              Space* space,
                              Object** write_back) {
  int size = source_->GetInt() << kObjectAlignmentBits;
  Address address = Allocate(space_number, space, size);
  *write_back = HeapObject::FromAddress(address);
  Object** current = reinterpret_cast<Object**>(address);
  Object** limit = current + (size >> kPointerSizeLog2);
  ReadChunk(current, limit, space_number, address);
}


#define ONE_CASE_PER_SPACE(base_tag)   \
  case (base_tag) + NEW_SPACE:         /* NOLINT */ \
  case (base_tag) + OLD_POINTER_SPACE: /* NOLINT */ \
  case (base_tag) + OLD_DATA_SPACE:    /* NOLINT */ \
  case (base_tag) + CODE_SPACE:        /* NOLINT */ \
  case (base_tag) + MAP_SPACE:         /* NOLINT */ \
  case (base_tag) + CELL_SPACE:        /* NOLINT */ \
  case (base_tag) + kLargeData:        /* NOLINT */ \
  case (base_tag) + kLargeCode:        /* NOLINT */ \
  case (base_tag) + kLargeFixedArray:  /* NOLINT */


void Deserializer::ReadChunk(Object** current,
                             Object** limit,
                             int space,
                             Address address) {
  while (current < limit) {
    int data = source_->Get();
    switch (data) {
#define RAW_CASE(index, size)                                      \
      case RAW_DATA_SERIALIZATION + index: {                       \
        byte* raw_data_out = reinterpret_cast<byte*>(current);     \
        source_->CopyRaw(raw_data_out, size);                      \
        current = reinterpret_cast<Object**>(raw_data_out + size); \
        break;                                                     \
      }
      COMMON_RAW_LENGTHS(RAW_CASE)
#undef RAW_CASE
      case RAW_DATA_SERIALIZATION: {
        int size = source_->GetInt();
        byte* raw_data_out = reinterpret_cast<byte*>(current);
        source_->CopyRaw(raw_data_out, size);
        current = reinterpret_cast<Object**>(raw_data_out + size);
        break;
      }
      case OBJECT_SERIALIZATION + NEW_SPACE: {
        ReadObject(NEW_SPACE, Heap::new_space(), current);
        if (space != NEW_SPACE) {
          Heap::RecordWrite(address, static_cast<int>(
              reinterpret_cast<Address>(current) - address));
        }
        current++;
        break;
      }
      case OBJECT_SERIALIZATION + OLD_DATA_SPACE:
        ReadObject(OLD_DATA_SPACE, Heap::old_data_space(), current++);
        break;
      case OBJECT_SERIALIZATION + OLD_POINTER_SPACE:
        ReadObject(OLD_POINTER_SPACE, Heap::old_pointer_space(), current++);
        break;
      case OBJECT_SERIALIZATION + MAP_SPACE:
        ReadObject(MAP_SPACE, Heap::map_space(), current++);
        break;
      case OBJECT_SERIALIZATION + CODE_SPACE:
        ReadObject(CODE_SPACE, Heap::code_space(), current++);
        LOG(LogCodeObject(current[-1]));
        break;
      case OBJECT_SERIALIZATION + CELL_SPACE:
        ReadObject(CELL_SPACE, Heap::cell_space(), current++);
        break;
      case OBJECT_SERIALIZATION + kLargeData:
        ReadObject(kLargeData, Heap::lo_space(), current++);
        break;
      case OBJECT_SERIALIZATION + kLargeCode:
        ReadObject(kLargeCode, Heap::lo_space(), current++);
        LOG(LogCodeObject(current[-1]));
        break;
      case OBJECT_SERIALIZATION + kLargeFixedArray:
        ReadObject(kLargeFixedArray, Heap::lo_space(), current++);
        break;
      case CODE_OBJECT_SERIALIZATION + kLargeCode: {
        Object* new_code_object = NULL;
        ReadObject(kLargeCode, Heap::lo_space(), &new_code_object);
        Code* code_object = reinterpret_cast<Code*>(new_code_object);
        LOG(LogCodeObject(code_object));
        // Setting a branch/call to another code object from code.
        Address location_of_branch_data = reinterpret_cast<Address>(current);
        Assembler::set_target_at(location_of_branch_data,
                                 code_object->instruction_start());
        location_of_branch_data += Assembler::kCallTargetSize;
        current = reinterpret_cast<Object**>(location_of_branch_data);
        break;
      }
      case CODE_OBJECT_SERIALIZATION + CODE_SPACE: {
        Object* new_code_object = NULL;
        ReadObject(CODE_SPACE, Heap::code_space(), &new_code_object);
        Code* code_object = reinterpret_cast<Code*>(new_code_object);
        LOG(LogCodeObject(code_object));
        // Setting a branch/call to another code object from code.
        Address location_of_branch_data = reinterpret_cast<Address>(current);
        Assembler::set_target_at(location_of_branch_data,
                                 code_object->instruction_start());
        location_of_branch_data += Assembler::kCallTargetSize;
        current = reinterpret_cast<Object**>(location_of_branch_data);
        break;
      }
      ONE_CASE_PER_SPACE(BACKREF_SERIALIZATION) {
        // Write a backreference to an object we unpacked earlier.
        int backref_space = (data & kSpaceMask);
        if (backref_space == NEW_SPACE && space != NEW_SPACE) {
          Heap::RecordWrite(address, static_cast<int>(
              reinterpret_cast<Address>(current) - address));
        }
        *current++ = GetAddressFromEnd(backref_space);
        break;
      }
      ONE_CASE_PER_SPACE(REFERENCE_SERIALIZATION) {
        // Write a reference to an object we unpacked earlier.
        int reference_space = (data & kSpaceMask);
        if (reference_space == NEW_SPACE && space != NEW_SPACE) {
          Heap::RecordWrite(address, static_cast<int>(
              reinterpret_cast<Address>(current) - address));
        }
        *current++ = GetAddressFromStart(reference_space);
        break;
      }
#define COMMON_REFS_CASE(index, reference_space, address)                      \
      case REFERENCE_SERIALIZATION + index: {                                  \
        ASSERT(SpaceIsPaged(reference_space));                                 \
        Address object_address =                                               \
            pages_[reference_space][0] + (address << kObjectAlignmentBits);    \
        *current++ = HeapObject::FromAddress(object_address);                  \
        break;                                                                 \
      }
      COMMON_REFERENCE_PATTERNS(COMMON_REFS_CASE)
#undef COMMON_REFS_CASE
      ONE_CASE_PER_SPACE(CODE_BACKREF_SERIALIZATION) {
        int backref_space = (data & kSpaceMask);
        // Can't use Code::cast because heap is not set up yet and assertions
        // will fail.
        Code* code_object =
            reinterpret_cast<Code*>(GetAddressFromEnd(backref_space));
        // Setting a branch/call to previously decoded code object from code.
        Address location_of_branch_data = reinterpret_cast<Address>(current);
        Assembler::set_target_at(location_of_branch_data,
                                 code_object->instruction_start());
        location_of_branch_data += Assembler::kCallTargetSize;
        current = reinterpret_cast<Object**>(location_of_branch_data);
        break;
      }
      ONE_CASE_PER_SPACE(CODE_REFERENCE_SERIALIZATION) {
        int backref_space = (data & kSpaceMask);
        // Can't use Code::cast because heap is not set up yet and assertions
        // will fail.
        Code* code_object =
            reinterpret_cast<Code*>(GetAddressFromStart(backref_space));
        // Setting a branch/call to previously decoded code object from code.
        Address location_of_branch_data = reinterpret_cast<Address>(current);
        Assembler::set_target_at(location_of_branch_data,
                                 code_object->instruction_start());
        location_of_branch_data += Assembler::kCallTargetSize;
        current = reinterpret_cast<Object**>(location_of_branch_data);
        break;
      }
      case EXTERNAL_REFERENCE_SERIALIZATION: {
        int reference_id = source_->GetInt();
        Address address = external_reference_decoder_->Decode(reference_id);
        *current++ = reinterpret_cast<Object*>(address);
        break;
      }
      case EXTERNAL_BRANCH_TARGET_SERIALIZATION: {
        int reference_id = source_->GetInt();
        Address address = external_reference_decoder_->Decode(reference_id);
        Address location_of_branch_data = reinterpret_cast<Address>(current);
        Assembler::set_external_target_at(location_of_branch_data, address);
        location_of_branch_data += Assembler::kExternalTargetSize;
        current = reinterpret_cast<Object**>(location_of_branch_data);
        break;
      }
      case START_NEW_PAGE_SERIALIZATION: {
        int space = source_->Get();
        pages_[space].Add(last_object_address_);
        break;
      }
      case NATIVES_STRING_RESOURCE: {
        int index = source_->Get();
        Vector<const char> source_vector = Natives::GetScriptSource(index);
        NativesExternalStringResource* resource =
            new NativesExternalStringResource(source_vector.start());
        *current++ = reinterpret_cast<Object*>(resource);
        break;
      }
      default:
        UNREACHABLE();
    }
  }
  ASSERT_EQ(current, limit);
}


void SnapshotByteSink::PutInt(uintptr_t integer, const char* description) {
  const int max_shift = ((kPointerSize * kBitsPerByte) / 7) * 7;
  for (int shift = max_shift; shift > 0; shift -= 7) {
    if (integer >= static_cast<uintptr_t>(1u) << shift) {
      Put(((integer >> shift) & 0x7f) | 0x80, "IntPart");
    }
  }
  PutSection(integer & 0x7f, "IntLastPart");
}

#ifdef DEBUG

void Deserializer::Synchronize(const char* tag) {
  int data = source_->Get();
  // If this assert fails then that indicates that you have a mismatch between
  // the number of GC roots when serializing and deserializing.
  ASSERT_EQ(SYNCHRONIZE, data);
  do {
    int character = source_->Get();
    if (character == 0) break;
    if (FLAG_debug_serialization) {
      PrintF("%c", character);
    }
  } while (true);
  if (FLAG_debug_serialization) {
    PrintF("\n");
  }
}


void Serializer::Synchronize(const char* tag) {
  sink_->Put(SYNCHRONIZE, tag);
  int character;
  do {
    character = *tag++;
    sink_->PutSection(character, "TagCharacter");
  } while (character != 0);
}

#endif

Serializer::Serializer(SnapshotByteSink* sink)
    : sink_(sink),
      current_root_index_(0),
      external_reference_encoder_(NULL) {
  for (int i = 0; i <= LAST_SPACE; i++) {
    fullness_[i] = 0;
  }
}


void Serializer::Serialize() {
  // No active threads.
  CHECK_EQ(NULL, ThreadState::FirstInUse());
  // No active or weak handles.
  CHECK(HandleScopeImplementer::instance()->blocks()->is_empty());
  CHECK_EQ(0, GlobalHandles::NumberOfWeakHandles());
  CHECK_EQ(NULL, external_reference_encoder_);
  // We don't support serializing installed extensions.
  for (RegisteredExtension* ext = RegisteredExtension::first_extension();
       ext != NULL;
       ext = ext->next()) {
    CHECK_NE(v8::INSTALLED, ext->state());
  }
  external_reference_encoder_ = new ExternalReferenceEncoder();
  Heap::IterateRoots(this, VISIT_ONLY_STRONG);
  delete external_reference_encoder_;
  external_reference_encoder_ = NULL;
  SerializationAddressMapper::Zap();
}


void Serializer::VisitPointers(Object** start, Object** end) {
  for (Object** current = start; current < end; current++) {
    if ((*current)->IsSmi()) {
      sink_->Put(RAW_DATA_SERIALIZATION, "RawData");
      sink_->PutInt(kPointerSize, "length");
      for (int i = 0; i < kPointerSize; i++) {
        sink_->Put(reinterpret_cast<byte*>(current)[i], "Byte");
      }
    } else {
      SerializeObject(*current, TAGGED_REPRESENTATION);
    }
  }
}


void Serializer::SerializeObject(
    Object* o,
    ReferenceRepresentation reference_representation) {
  CHECK(o->IsHeapObject());
  HeapObject* heap_object = HeapObject::cast(o);
  if (SerializationAddressMapper::IsMapped(heap_object)) {
    int space = SpaceOfAlreadySerializedObject(heap_object);
    int address = SerializationAddressMapper::MappedTo(heap_object);
    int offset = CurrentAllocationAddress(space) - address;
    bool from_start = true;
    if (SpaceIsPaged(space)) {
      if ((CurrentAllocationAddress(space) >> Page::kPageSizeBits) ==
          (address >> Page::kPageSizeBits)) {
        from_start = false;
        address = offset;
      }
    } else if (space == NEW_SPACE) {
      if (offset < address) {
        from_start = false;
        address = offset;
      }
    }
    // If we are actually dealing with real offsets (and not a numbering of
    // all objects) then we should shift out the bits that are always 0.
    if (!SpaceIsLarge(space)) address >>= kObjectAlignmentBits;
    if (reference_representation == CODE_TARGET_REPRESENTATION) {
      if (from_start) {
        sink_->Put(CODE_REFERENCE_SERIALIZATION + space, "RefCodeSer");
        sink_->PutInt(address, "address");
      } else {
        sink_->Put(CODE_BACKREF_SERIALIZATION + space, "BackRefCodeSer");
        sink_->PutInt(address, "address");
      }
    } else {
      CHECK_EQ(TAGGED_REPRESENTATION, reference_representation);
      if (from_start) {
#define COMMON_REFS_CASE(tag, common_space, common_offset)                 \
        if (space == common_space && address == common_offset) {           \
          sink_->PutSection(tag + REFERENCE_SERIALIZATION, "RefSer");      \
        } else  /* NOLINT */
        COMMON_REFERENCE_PATTERNS(COMMON_REFS_CASE)
#undef COMMON_REFS_CASE
        {  /* NOLINT */
          sink_->Put(REFERENCE_SERIALIZATION + space, "RefSer");
          sink_->PutInt(address, "address");
        }
      } else {
        sink_->Put(BACKREF_SERIALIZATION + space, "BackRefSer");
        sink_->PutInt(address, "address");
      }
    }
  } else {
    // Object has not yet been serialized.  Serialize it here.
    ObjectSerializer serializer(this,
                                heap_object,
                                sink_,
                                reference_representation);
    serializer.Serialize();
  }
}



void Serializer::ObjectSerializer::Serialize() {
  int space = Serializer::SpaceOfObject(object_);
  int size = object_->Size();

  if (reference_representation_ == TAGGED_REPRESENTATION) {
    sink_->Put(OBJECT_SERIALIZATION + space, "ObjectSerialization");
  } else {
    CHECK_EQ(CODE_TARGET_REPRESENTATION, reference_representation_);
    sink_->Put(CODE_OBJECT_SERIALIZATION + space, "ObjectSerialization");
  }
  sink_->PutInt(size >> kObjectAlignmentBits, "Size in words");

  // Mark this object as already serialized.
  bool start_new_page;
  SerializationAddressMapper::Map(
    object_,
    serializer_->Allocate(space, size, &start_new_page));
  if (start_new_page) {
    sink_->Put(START_NEW_PAGE_SERIALIZATION, "NewPage");
    sink_->PutSection(space, "NewPageSpace");
  }

  // Serialize the map (first word of the object).
  serializer_->SerializeObject(object_->map(), TAGGED_REPRESENTATION);

  // Serialize the rest of the object.
  CHECK_EQ(0, bytes_processed_so_far_);
  bytes_processed_so_far_ = kPointerSize;
  object_->IterateBody(object_->map()->instance_type(), size, this);
  OutputRawData(object_->address() + size);
}


void Serializer::ObjectSerializer::VisitPointers(Object** start,
                                                 Object** end) {
  Object** current = start;
  while (current < end) {
    while (current < end && (*current)->IsSmi()) current++;
    if (current < end) OutputRawData(reinterpret_cast<Address>(current));

    while (current < end && !(*current)->IsSmi()) {
      serializer_->SerializeObject(*current, TAGGED_REPRESENTATION);
      bytes_processed_so_far_ += kPointerSize;
      current++;
    }
  }
}


void Serializer::ObjectSerializer::VisitExternalReferences(Address* start,
                                                           Address* end) {
  Address references_start = reinterpret_cast<Address>(start);
  OutputRawData(references_start);

  for (Address* current = start; current < end; current++) {
    sink_->Put(EXTERNAL_REFERENCE_SERIALIZATION, "ExternalReference");
    int reference_id = serializer_->EncodeExternalReference(*current);
    sink_->PutInt(reference_id, "reference id");
  }
  bytes_processed_so_far_ += static_cast<int>((end - start) * kPointerSize);
}


void Serializer::ObjectSerializer::VisitRuntimeEntry(RelocInfo* rinfo) {
  Address target_start = rinfo->target_address_address();
  OutputRawData(target_start);
  Address target = rinfo->target_address();
  uint32_t encoding = serializer_->EncodeExternalReference(target);
  CHECK(target == NULL ? encoding == 0 : encoding != 0);
  sink_->Put(EXTERNAL_BRANCH_TARGET_SERIALIZATION, "ExternalReference");
  sink_->PutInt(encoding, "reference id");
  bytes_processed_so_far_ += Assembler::kExternalTargetSize;
}


void Serializer::ObjectSerializer::VisitCodeTarget(RelocInfo* rinfo) {
  CHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
  Address target_start = rinfo->target_address_address();
  OutputRawData(target_start);
  Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  serializer_->SerializeObject(target, CODE_TARGET_REPRESENTATION);
  bytes_processed_so_far_ += Assembler::kCallTargetSize;
}


void Serializer::ObjectSerializer::VisitExternalAsciiString(
    v8::String::ExternalAsciiStringResource** resource_pointer) {
  Address references_start = reinterpret_cast<Address>(resource_pointer);
  OutputRawData(references_start);
  for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
    Object* source = Heap::natives_source_cache()->get(i);
    if (!source->IsUndefined()) {
      ExternalAsciiString* string = ExternalAsciiString::cast(source);
      typedef v8::String::ExternalAsciiStringResource Resource;
      Resource* resource = string->resource();
      if (resource == *resource_pointer) {
        sink_->Put(NATIVES_STRING_RESOURCE, "NativesStringResource");
        sink_->PutSection(i, "NativesStringResourceEnd");
        bytes_processed_so_far_ += sizeof(resource);
        return;
      }
    }
  }
  // One of the strings in the natives cache should match the resource.  We
  // can't serialize any other kinds of external strings.
  UNREACHABLE();
}


void Serializer::ObjectSerializer::OutputRawData(Address up_to) {
  Address object_start = object_->address();
  int up_to_offset = static_cast<int>(up_to - object_start);
  int skipped = up_to_offset - bytes_processed_so_far_;
  // This assert will fail if the reloc info gives us the target_address_address
  // locations in a non-ascending order.  Luckily that doesn't happen.
  ASSERT(skipped >= 0);
  if (skipped != 0) {
    Address base = object_start + bytes_processed_so_far_;
#define RAW_CASE(index, length)                                                \
    if (skipped == length) {                                                   \
      sink_->PutSection(RAW_DATA_SERIALIZATION + index, "RawDataFixed");       \
    } else  /* NOLINT */
    COMMON_RAW_LENGTHS(RAW_CASE)
#undef RAW_CASE
    {  /* NOLINT */
      sink_->Put(RAW_DATA_SERIALIZATION, "RawData");
      sink_->PutInt(skipped, "length");
    }
    for (int i = 0; i < skipped; i++) {
      unsigned int data = base[i];
      sink_->PutSection(data, "Byte");
    }
    bytes_processed_so_far_ += skipped;
  }
}


int Serializer::SpaceOfObject(HeapObject* object) {
  for (int i = FIRST_SPACE; i <= LAST_SPACE; i++) {
    AllocationSpace s = static_cast<AllocationSpace>(i);
    if (Heap::InSpace(object, s)) {
      if (i == LO_SPACE) {
        if (object->IsCode()) {
          return kLargeCode;
        } else if (object->IsFixedArray()) {
          return kLargeFixedArray;
        } else {
          return kLargeData;
        }
      }
      return i;
    }
  }
  UNREACHABLE();
  return 0;
}


int Serializer::SpaceOfAlreadySerializedObject(HeapObject* object) {
  for (int i = FIRST_SPACE; i <= LAST_SPACE; i++) {
    AllocationSpace s = static_cast<AllocationSpace>(i);
    if (Heap::InSpace(object, s)) {
      return i;
    }
  }
  UNREACHABLE();
  return 0;
}


int Serializer::Allocate(int space, int size, bool* new_page) {
  CHECK(space >= 0 && space < kNumberOfSpaces);
  if (SpaceIsLarge(space)) {
    // In large object space we merely number the objects instead of trying to
    // determine some sort of address.
    *new_page = true;
    return fullness_[LO_SPACE]++;
  }
  *new_page = false;
  if (fullness_[space] == 0) {
    *new_page = true;
  }
  if (SpaceIsPaged(space)) {
    // Paged spaces are a little special.  We encode their addresses as if the
    // pages were all contiguous and each page were filled up in the range
    // 0 - Page::kObjectAreaSize.  In practice the pages may not be contiguous
    // and allocation does not start at offset 0 in the page, but this scheme
    // means the deserializer can get the page number quickly by shifting the
    // serialized address.
    CHECK(IsPowerOf2(Page::kPageSize));
    int used_in_this_page = (fullness_[space] & (Page::kPageSize - 1));
    CHECK(size <= Page::kObjectAreaSize);
    if (used_in_this_page + size > Page::kObjectAreaSize) {
      *new_page = true;
      fullness_[space] = RoundUp(fullness_[space], Page::kPageSize);
    }
  }
  int allocation_address = fullness_[space];
  fullness_[space] = allocation_address + size;
  return allocation_address;
}


} }  // namespace v8::internal
