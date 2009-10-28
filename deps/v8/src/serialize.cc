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

namespace v8 {
namespace internal {

// 32-bit encoding: a RelativeAddress must be able to fit in a
// pointer: it is encoded as an Address with (from LS to MS bits):
// - 2 bits identifying this as a HeapObject.
// - 4 bits to encode the AllocationSpace (including special values for
//   code and fixed arrays in LO space)
// - 27 bits identifying a word in the space, in one of three formats:
// - paged spaces: 16 bits of page number, 11 bits of word offset in page
// - NEW space:    27 bits of word offset
// - LO space:     27 bits of page number

const int kSpaceShift = kHeapObjectTagSize;
const int kSpaceBits = 4;
const int kSpaceMask = (1 << kSpaceBits) - 1;

const int kOffsetShift = kSpaceShift + kSpaceBits;
const int kOffsetBits = 11;
const int kOffsetMask = (1 << kOffsetBits) - 1;

const int kPageShift = kOffsetShift + kOffsetBits;
const int kPageBits = 32 - (kOffsetBits + kSpaceBits + kHeapObjectTagSize);
const int kPageMask = (1 << kPageBits) - 1;

const int kPageAndOffsetShift = kOffsetShift;
const int kPageAndOffsetBits = kPageBits + kOffsetBits;
const int kPageAndOffsetMask = (1 << kPageAndOffsetBits) - 1;

// These values are special allocation space tags used for
// serialization.
// Mark the pages executable on platforms that support it.
const int kLargeCode = LAST_SPACE + 1;
// Allocate extra remembered-set bits.
const int kLargeFixedArray = LAST_SPACE + 2;


static inline AllocationSpace GetSpace(Address addr) {
  const intptr_t encoded = reinterpret_cast<intptr_t>(addr);
  int space_number = (static_cast<int>(encoded >> kSpaceShift) & kSpaceMask);
  if (space_number > LAST_SPACE) space_number = LO_SPACE;
  return static_cast<AllocationSpace>(space_number);
}


static inline bool IsLargeExecutableObject(Address addr) {
  const intptr_t encoded = reinterpret_cast<intptr_t>(addr);
  const int space_number =
      (static_cast<int>(encoded >> kSpaceShift) & kSpaceMask);
  return (space_number == kLargeCode);
}


static inline bool IsLargeFixedArray(Address addr) {
  const intptr_t encoded = reinterpret_cast<intptr_t>(addr);
  const int space_number =
      (static_cast<int>(encoded >> kSpaceShift) & kSpaceMask);
  return (space_number == kLargeFixedArray);
}


static inline int PageIndex(Address addr) {
  const intptr_t encoded = reinterpret_cast<intptr_t>(addr);
  return static_cast<int>(encoded >> kPageShift) & kPageMask;
}


static inline int PageOffset(Address addr) {
  const intptr_t encoded = reinterpret_cast<intptr_t>(addr);
  const int offset = static_cast<int>(encoded >> kOffsetShift) & kOffsetMask;
  return offset << kObjectAlignmentBits;
}


static inline int NewSpaceOffset(Address addr) {
  const intptr_t encoded = reinterpret_cast<intptr_t>(addr);
  const int page_offset =
      static_cast<int>(encoded >> kPageAndOffsetShift) & kPageAndOffsetMask;
  return page_offset << kObjectAlignmentBits;
}


static inline int LargeObjectIndex(Address addr) {
  const intptr_t encoded = reinterpret_cast<intptr_t>(addr);
  return static_cast<int>(encoded >> kPageAndOffsetShift) & kPageAndOffsetMask;
}


// A RelativeAddress encodes a heap address that is independent of
// the actual memory addresses in real heap. The general case (for the
// OLD, CODE and MAP spaces) is as a (space id, page number, page offset)
// triple. The NEW space has page number == 0, because there are no
// pages. The LARGE_OBJECT space has page offset = 0, since there is
// exactly one object per page.  RelativeAddresses are encodable as
// Addresses, so that they can replace the map() pointers of
// HeapObjects. The encoded Addresses are also encoded as HeapObjects
// and allow for marking (is_marked() see mark(), clear_mark()...) as
// used by the Mark-Compact collector.

class RelativeAddress {
 public:
  RelativeAddress(AllocationSpace space,
                  int page_index,
                  int page_offset)
  : space_(space), page_index_(page_index), page_offset_(page_offset)  {
    // Assert that the space encoding (plus the two pseudo-spaces for
    // special large objects) fits in the available bits.
    ASSERT(((LAST_SPACE + 2) & ~kSpaceMask) == 0);
    ASSERT(space <= LAST_SPACE && space >= 0);
  }

  // Return the encoding of 'this' as an Address. Decode with constructor.
  Address Encode() const;

  AllocationSpace space() const {
    if (space_ > LAST_SPACE) return LO_SPACE;
    return static_cast<AllocationSpace>(space_);
  }
  int page_index() const { return page_index_; }
  int page_offset() const { return page_offset_; }

  bool in_paged_space() const {
    return space_ == CODE_SPACE ||
           space_ == OLD_POINTER_SPACE ||
           space_ == OLD_DATA_SPACE ||
           space_ == MAP_SPACE ||
           space_ == CELL_SPACE;
  }

  void next_address(int offset) { page_offset_ += offset; }
  void next_page(int init_offset = 0) {
    page_index_++;
    page_offset_ = init_offset;
  }

#ifdef DEBUG
  void Verify();
#endif

  void set_to_large_code_object() {
    ASSERT(space_ == LO_SPACE);
    space_ = kLargeCode;
  }
  void set_to_large_fixed_array() {
    ASSERT(space_ == LO_SPACE);
    space_ = kLargeFixedArray;
  }


 private:
  int space_;
  int page_index_;
  int page_offset_;
};


Address RelativeAddress::Encode() const {
  ASSERT(page_index_ >= 0);
  int word_offset = 0;
  int result = 0;
  switch (space_) {
    case MAP_SPACE:
    case CELL_SPACE:
    case OLD_POINTER_SPACE:
    case OLD_DATA_SPACE:
    case CODE_SPACE:
      ASSERT_EQ(0, page_index_ & ~kPageMask);
      word_offset = page_offset_ >> kObjectAlignmentBits;
      ASSERT_EQ(0, word_offset & ~kOffsetMask);
      result = (page_index_ << kPageShift) | (word_offset << kOffsetShift);
      break;
    case NEW_SPACE:
      ASSERT_EQ(0, page_index_);
      word_offset = page_offset_ >> kObjectAlignmentBits;
      ASSERT_EQ(0, word_offset & ~kPageAndOffsetMask);
      result = word_offset << kPageAndOffsetShift;
      break;
    case LO_SPACE:
    case kLargeCode:
    case kLargeFixedArray:
      ASSERT_EQ(0, page_offset_);
      ASSERT_EQ(0, page_index_ & ~kPageAndOffsetMask);
      result = page_index_ << kPageAndOffsetShift;
      break;
  }
  // OR in AllocationSpace and kHeapObjectTag
  ASSERT_EQ(0, space_ & ~kSpaceMask);
  result |= (space_ << kSpaceShift) | kHeapObjectTag;
  return reinterpret_cast<Address>(result);
}


#ifdef DEBUG
void RelativeAddress::Verify() {
  ASSERT(page_offset_ >= 0 && page_index_ >= 0);
  switch (space_) {
    case MAP_SPACE:
    case CELL_SPACE:
    case OLD_POINTER_SPACE:
    case OLD_DATA_SPACE:
    case CODE_SPACE:
      ASSERT(Page::kObjectStartOffset <= page_offset_ &&
             page_offset_ <= Page::kPageSize);
      break;
    case NEW_SPACE:
      ASSERT(page_index_ == 0);
      break;
    case LO_SPACE:
    case kLargeCode:
    case kLargeFixedArray:
      ASSERT(page_offset_ == 0);
      break;
  }
}
#endif

enum GCTreatment {
  DataObject,     // Object that cannot contain a reference to new space.
  PointerObject,  // Object that can contain a reference to new space.
  CodeObject      // Object that contains executable code.
};

// A SimulatedHeapSpace simulates the allocation of objects in a page in
// the heap. It uses linear allocation - that is, it doesn't simulate the
// use of a free list. This simulated
// allocation must exactly match that done by Heap.

class SimulatedHeapSpace {
 public:
  // The default constructor initializes to an invalid state.
  SimulatedHeapSpace(): current_(LAST_SPACE, -1, -1) {}

  // Sets 'this' to the first address in 'space' that would be
  // returned by allocation in an empty heap.
  void InitEmptyHeap(AllocationSpace space);

  // Sets 'this' to the next address in 'space' that would be returned
  // by allocation in the current heap. Intended only for testing
  // serialization and deserialization in the current address space.
  void InitCurrentHeap(AllocationSpace space);

  // Returns the RelativeAddress where the next
  // object of 'size' bytes will be allocated, and updates 'this' to
  // point to the next free address beyond that object.
  RelativeAddress Allocate(int size, GCTreatment special_gc_treatment);

 private:
  RelativeAddress current_;
};


void SimulatedHeapSpace::InitEmptyHeap(AllocationSpace space) {
  switch (space) {
    case MAP_SPACE:
    case CELL_SPACE:
    case OLD_POINTER_SPACE:
    case OLD_DATA_SPACE:
    case CODE_SPACE:
      current_ = RelativeAddress(space, 0, Page::kObjectStartOffset);
      break;
    case NEW_SPACE:
    case LO_SPACE:
      current_ = RelativeAddress(space, 0, 0);
      break;
  }
}


void SimulatedHeapSpace::InitCurrentHeap(AllocationSpace space) {
  switch (space) {
    case MAP_SPACE:
    case CELL_SPACE:
    case OLD_POINTER_SPACE:
    case OLD_DATA_SPACE:
    case CODE_SPACE: {
      PagedSpace* ps;
      if (space == MAP_SPACE) {
        ps = Heap::map_space();
      } else if (space == CELL_SPACE) {
        ps = Heap::cell_space();
      } else if (space == OLD_POINTER_SPACE) {
        ps = Heap::old_pointer_space();
      } else if (space == OLD_DATA_SPACE) {
        ps = Heap::old_data_space();
      } else {
        ASSERT(space == CODE_SPACE);
        ps = Heap::code_space();
      }
      Address top = ps->top();
      Page* top_page = Page::FromAllocationTop(top);
      int page_index = 0;
      PageIterator it(ps, PageIterator::PAGES_IN_USE);
      while (it.has_next()) {
        if (it.next() == top_page) break;
        page_index++;
      }
      current_ = RelativeAddress(space,
                                 page_index,
                                 top_page->Offset(top));
      break;
    }
    case NEW_SPACE:
      current_ = RelativeAddress(space,
                                 0,
                                 Heap::NewSpaceTop() - Heap::NewSpaceStart());
      break;
    case LO_SPACE:
      int page_index = 0;
      for (LargeObjectIterator it(Heap::lo_space()); it.has_next(); it.next()) {
        page_index++;
      }
      current_ = RelativeAddress(space, page_index, 0);
      break;
  }
}


RelativeAddress SimulatedHeapSpace::Allocate(int size,
                                             GCTreatment special_gc_treatment) {
#ifdef DEBUG
  current_.Verify();
#endif
  int alloc_size = OBJECT_SIZE_ALIGN(size);
  if (current_.in_paged_space() &&
      current_.page_offset() + alloc_size > Page::kPageSize) {
    ASSERT(alloc_size <= Page::kMaxHeapObjectSize);
    current_.next_page(Page::kObjectStartOffset);
  }
  RelativeAddress result = current_;
  if (current_.space() == LO_SPACE) {
    current_.next_page();
    if (special_gc_treatment == CodeObject) {
      result.set_to_large_code_object();
    } else if (special_gc_treatment == PointerObject) {
      result.set_to_large_fixed_array();
    }
  } else {
    current_.next_address(alloc_size);
  }
#ifdef DEBUG
  current_.Verify();
  result.Verify();
#endif
  return result;
}

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
  CHECK_NE(NULL, address);
  ExternalReferenceEntry entry;
  entry.address = address;
  entry.code = EncodeExternal(type, id);
  entry.name = name;
  CHECK_NE(0, entry.code);
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
  size_t dr_format_length = strlen(debug_register_format);
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

  size_t top_format_length = strlen(top_address_format) - 2;
  for (uint16_t i = 0; i < Top::k_top_address_count; ++i) {
    const char* address_name = AddressNames[i];
    Vector<char> name =
        Vector<char>::New(top_format_length + strlen(address_name) + 1);
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
  Add(ExternalReference::address_of_stack_guard_limit().address(),
      UNCLASSIFIED,
      4,
      "StackGuard::address_of_jslimit()");
  Add(ExternalReference::address_of_regexp_stack_limit().address(),
      UNCLASSIFIED,
      5,
      "RegExpStack::limit_address()");
  Add(ExternalReference::new_space_start().address(),
      UNCLASSIFIED,
      6,
      "Heap::NewSpaceStart()");
  Add(ExternalReference::heap_always_allocate_scope_depth().address(),
      UNCLASSIFIED,
      7,
      "Heap::always_allocate_scope_depth()");
  Add(ExternalReference::new_space_allocation_limit_address().address(),
      UNCLASSIFIED,
      8,
      "Heap::NewSpaceAllocationLimitAddress()");
  Add(ExternalReference::new_space_allocation_top_address().address(),
      UNCLASSIFIED,
      9,
      "Heap::NewSpaceAllocationTopAddress()");
#ifdef ENABLE_DEBUGGER_SUPPORT
  Add(ExternalReference::debug_break().address(),
      UNCLASSIFIED,
      10,
      "Debug::Break()");
  Add(ExternalReference::debug_step_in_fp_address().address(),
      UNCLASSIFIED,
      11,
      "Debug::step_in_fp_addr()");
#endif
  Add(ExternalReference::double_fp_operation(Token::ADD).address(),
      UNCLASSIFIED,
      12,
      "add_two_doubles");
  Add(ExternalReference::double_fp_operation(Token::SUB).address(),
      UNCLASSIFIED,
      13,
      "sub_two_doubles");
  Add(ExternalReference::double_fp_operation(Token::MUL).address(),
      UNCLASSIFIED,
      14,
      "mul_two_doubles");
  Add(ExternalReference::double_fp_operation(Token::DIV).address(),
      UNCLASSIFIED,
      15,
      "div_two_doubles");
  Add(ExternalReference::double_fp_operation(Token::MOD).address(),
      UNCLASSIFIED,
      16,
      "mod_two_doubles");
  Add(ExternalReference::compare_doubles().address(),
      UNCLASSIFIED,
      17,
      "compare_doubles");
#ifdef V8_NATIVE_REGEXP
  Add(ExternalReference::re_case_insensitive_compare_uc16().address(),
      UNCLASSIFIED,
      18,
      "NativeRegExpMacroAssembler::CaseInsensitiveCompareUC16()");
  Add(ExternalReference::re_check_stack_guard_state().address(),
      UNCLASSIFIED,
      19,
      "RegExpMacroAssembler*::CheckStackGuardState()");
  Add(ExternalReference::re_grow_stack().address(),
      UNCLASSIFIED,
      20,
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


//------------------------------------------------------------------------------
// Implementation of Serializer


// Helper class to write the bytes of the serialized heap.

class SnapshotWriter {
 public:
  SnapshotWriter() {
    len_ = 0;
    max_ = 8 << 10;  // 8K initial size
    str_ = NewArray<byte>(max_);
  }

  ~SnapshotWriter() {
    DeleteArray(str_);
  }

  void GetBytes(byte** str, int* len) {
    *str = NewArray<byte>(len_);
    memcpy(*str, str_, len_);
    *len = len_;
  }

  void Reserve(int bytes, int pos);

  void PutC(char c) {
    InsertC(c, len_);
  }

  void PutInt(int i) {
    InsertInt(i, len_);
  }

  void PutAddress(Address p) {
    PutBytes(reinterpret_cast<byte*>(&p), sizeof(p));
  }

  void PutBytes(const byte* a, int size) {
    InsertBytes(a, len_, size);
  }

  void PutString(const char* s) {
    InsertString(s, len_);
  }

  int InsertC(char c, int pos) {
    Reserve(1, pos);
    str_[pos] = c;
    len_++;
    return pos + 1;
  }

  int InsertInt(int i, int pos) {
    return InsertBytes(reinterpret_cast<byte*>(&i), pos, sizeof(i));
  }

  int InsertBytes(const byte* a, int pos, int size) {
    Reserve(size, pos);
    memcpy(&str_[pos], a, size);
    len_ += size;
    return pos + size;
  }

  int InsertString(const char* s, int pos);

  int length() { return len_; }

  Address position() { return reinterpret_cast<Address>(&str_[len_]); }

 private:
  byte* str_;  // the snapshot
  int len_;   // the current length of str_
  int max_;   // the allocated size of str_
};


void SnapshotWriter::Reserve(int bytes, int pos) {
  CHECK(0 <= pos && pos <= len_);
  while (len_ + bytes >= max_) {
    max_ *= 2;
    byte* old = str_;
    str_ = NewArray<byte>(max_);
    memcpy(str_, old, len_);
    DeleteArray(old);
  }
  if (pos < len_) {
    byte* old = str_;
    str_ = NewArray<byte>(max_);
    memcpy(str_, old, pos);
    memcpy(str_ + pos + bytes, old + pos, len_ - pos);
    DeleteArray(old);
  }
}

int SnapshotWriter::InsertString(const char* s, int pos) {
  int size = strlen(s);
  pos = InsertC('[', pos);
  pos = InsertInt(size, pos);
  pos = InsertC(']', pos);
  return InsertBytes(reinterpret_cast<const byte*>(s), pos, size);
}


class ReferenceUpdater: public ObjectVisitor {
 public:
  ReferenceUpdater(HeapObject* obj, Serializer* serializer)
    : obj_address_(obj->address()),
      serializer_(serializer),
      reference_encoder_(serializer->reference_encoder_),
      offsets_(8),
      addresses_(8),
      offsets_32_bit_(0),
      data_32_bit_(0) {
  }

  virtual void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; ++p) {
      if ((*p)->IsHeapObject()) {
        offsets_.Add(reinterpret_cast<Address>(p) - obj_address_);
        Address a = serializer_->GetSavedAddress(HeapObject::cast(*p));
        addresses_.Add(a);
      }
    }
  }

  virtual void VisitCodeTarget(RelocInfo* rinfo) {
    ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    Address encoded_target = serializer_->GetSavedAddress(target);
    // All calls and jumps are to code objects that encode into 32 bits.
    offsets_32_bit_.Add(rinfo->target_address_address() - obj_address_);
    uint32_t small_target =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(encoded_target));
    ASSERT(reinterpret_cast<uintptr_t>(encoded_target) == small_target);
    data_32_bit_.Add(small_target);
  }


  virtual void VisitExternalReferences(Address* start, Address* end) {
    for (Address* p = start; p < end; ++p) {
      uint32_t code = reference_encoder_->Encode(*p);
      CHECK(*p == NULL ? code == 0 : code != 0);
      offsets_.Add(reinterpret_cast<Address>(p) - obj_address_);
      addresses_.Add(reinterpret_cast<Address>(code));
    }
  }

  virtual void VisitRuntimeEntry(RelocInfo* rinfo) {
    Address target = rinfo->target_address();
    uint32_t encoding = reference_encoder_->Encode(target);
    CHECK(target == NULL ? encoding == 0 : encoding != 0);
    offsets_.Add(rinfo->target_address_address() - obj_address_);
    addresses_.Add(reinterpret_cast<Address>(encoding));
  }

  void Update(Address start_address) {
    for (int i = 0; i < offsets_.length(); i++) {
      memcpy(start_address + offsets_[i], &addresses_[i], sizeof(Address));
    }
    for (int i = 0; i < offsets_32_bit_.length(); i++) {
      memcpy(start_address + offsets_32_bit_[i], &data_32_bit_[i],
             sizeof(uint32_t));
    }
  }

 private:
  Address obj_address_;
  Serializer* serializer_;
  ExternalReferenceEncoder* reference_encoder_;
  List<int> offsets_;
  List<Address> addresses_;
  // Some updates are 32-bit even on a 64-bit platform.
  // We keep a separate list of them on 64-bit platforms.
  List<int> offsets_32_bit_;
  List<uint32_t> data_32_bit_;
};


// Helper functions for a map of encoded heap object addresses.
static uint32_t HeapObjectHash(HeapObject* key) {
  uint32_t low32bits = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(key));
  return low32bits >> 2;
}


static bool MatchHeapObject(void* key1, void* key2) {
  return key1 == key2;
}


Serializer::Serializer()
  : global_handles_(4),
    saved_addresses_(MatchHeapObject) {
  root_ = true;
  roots_ = 0;
  objects_ = 0;
  reference_encoder_ = NULL;
  writer_ = new SnapshotWriter();
  for (int i = 0; i <= LAST_SPACE; i++) {
    allocator_[i] = new SimulatedHeapSpace();
  }
}


Serializer::~Serializer() {
  for (int i = 0; i <= LAST_SPACE; i++) {
    delete allocator_[i];
  }
  if (reference_encoder_) delete reference_encoder_;
  delete writer_;
}


bool Serializer::serialization_enabled_ = false;


#ifdef DEBUG
static const int kMaxTagLength = 32;

void Serializer::Synchronize(const char* tag) {
  if (FLAG_debug_serialization) {
    int length = strlen(tag);
    ASSERT(length <= kMaxTagLength);
    writer_->PutC('S');
    writer_->PutInt(length);
    writer_->PutBytes(reinterpret_cast<const byte*>(tag), length);
  }
}
#endif


void Serializer::InitializeAllocators() {
  for (int i = 0; i <= LAST_SPACE; i++) {
    allocator_[i]->InitEmptyHeap(static_cast<AllocationSpace>(i));
  }
}


bool Serializer::IsVisited(HeapObject* obj) {
  HashMap::Entry* entry =
    saved_addresses_.Lookup(obj, HeapObjectHash(obj), false);
  return entry != NULL;
}


Address Serializer::GetSavedAddress(HeapObject* obj) {
  HashMap::Entry* entry =
    saved_addresses_.Lookup(obj, HeapObjectHash(obj), false);
  ASSERT(entry != NULL);
  return reinterpret_cast<Address>(entry->value);
}


void Serializer::SaveAddress(HeapObject* obj, Address addr) {
  HashMap::Entry* entry =
    saved_addresses_.Lookup(obj, HeapObjectHash(obj), true);
  entry->value = addr;
}


void Serializer::Serialize() {
  // No active threads.
  CHECK_EQ(NULL, ThreadState::FirstInUse());
  // No active or weak handles.
  CHECK(HandleScopeImplementer::instance()->blocks()->is_empty());
  CHECK_EQ(0, GlobalHandles::NumberOfWeakHandles());
  // We need a counter function during serialization to resolve the
  // references to counters in the code on the heap.
  CHECK(StatsTable::HasCounterFunction());
  CHECK(enabled());
  InitializeAllocators();
  reference_encoder_ = new ExternalReferenceEncoder();
  PutHeader();
  Heap::IterateRoots(this);
  PutLog();
  PutContextStack();
  Disable();
}


void Serializer::Finalize(byte** str, int* len) {
  writer_->GetBytes(str, len);
}


// Serialize objects by writing them into the stream.

void Serializer::VisitPointers(Object** start, Object** end) {
  bool root = root_;
  root_ = false;
  for (Object** p = start; p < end; ++p) {
    bool serialized;
    Address a = Encode(*p, &serialized);
    if (root) {
      roots_++;
      // If the object was not just serialized,
      // write its encoded address instead.
      if (!serialized) PutEncodedAddress(a);
    }
  }
  root_ = root;
}


void Serializer::VisitCodeTarget(RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
  Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  bool serialized;
  Encode(target, &serialized);
}


class GlobalHandlesRetriever: public ObjectVisitor {
 public:
  explicit GlobalHandlesRetriever(List<Object**>* handles)
  : global_handles_(handles) {}

  virtual void VisitPointers(Object** start, Object** end) {
    for (; start != end; ++start) {
      global_handles_->Add(start);
    }
  }

 private:
  List<Object**>* global_handles_;
};


void Serializer::PutFlags() {
  writer_->PutC('F');
  List<const char*>* argv = FlagList::argv();
  writer_->PutInt(argv->length());
  writer_->PutC('[');
  for (int i = 0; i < argv->length(); i++) {
    if (i > 0) writer_->PutC('|');
    writer_->PutString((*argv)[i]);
    DeleteArray((*argv)[i]);
  }
  writer_->PutC(']');
  flags_end_ = writer_->length();
  delete argv;
}


void Serializer::PutHeader() {
  PutFlags();
  writer_->PutC('D');
#ifdef DEBUG
  writer_->PutC(FLAG_debug_serialization ? '1' : '0');
#else
  writer_->PutC('0');
#endif
#ifdef V8_NATIVE_REGEXP
  writer_->PutC('N');
#else  // Interpreted regexp
  writer_->PutC('I');
#endif
  // Write sizes of paged memory spaces. Allocate extra space for the old
  // and code spaces, because objects in new space will be promoted to them.
  writer_->PutC('S');
  writer_->PutC('[');
  writer_->PutInt(Heap::old_pointer_space()->Size() +
                  Heap::new_space()->Size());
  writer_->PutC('|');
  writer_->PutInt(Heap::old_data_space()->Size() + Heap::new_space()->Size());
  writer_->PutC('|');
  writer_->PutInt(Heap::code_space()->Size() + Heap::new_space()->Size());
  writer_->PutC('|');
  writer_->PutInt(Heap::map_space()->Size());
  writer_->PutC('|');
  writer_->PutInt(Heap::cell_space()->Size());
  writer_->PutC(']');
  // Write global handles.
  writer_->PutC('G');
  writer_->PutC('[');
  GlobalHandlesRetriever ghr(&global_handles_);
  GlobalHandles::IterateRoots(&ghr);
  for (int i = 0; i < global_handles_.length(); i++) {
    writer_->PutC('N');
  }
  writer_->PutC(']');
}


void Serializer::PutLog() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (FLAG_log_code) {
    Logger::TearDown();
    int pos = writer_->InsertC('L', flags_end_);
    bool exists;
    Vector<const char> log = ReadFile(FLAG_logfile, &exists);
    writer_->InsertString(log.start(), pos);
    log.Dispose();
  }
#endif
}


static int IndexOf(const List<Object**>& list, Object** element) {
  for (int i = 0; i < list.length(); i++) {
    if (list[i] == element) return i;
  }
  return -1;
}


void Serializer::PutGlobalHandleStack(const List<Handle<Object> >& stack) {
  writer_->PutC('[');
  writer_->PutInt(stack.length());
  for (int i = stack.length() - 1; i >= 0; i--) {
    writer_->PutC('|');
    int gh_index = IndexOf(global_handles_, stack[i].location());
    CHECK_GE(gh_index, 0);
    writer_->PutInt(gh_index);
  }
  writer_->PutC(']');
}


void Serializer::PutContextStack() {
  List<Context*> contexts(2);
  while (HandleScopeImplementer::instance()->HasSavedContexts()) {
    Context* context =
      HandleScopeImplementer::instance()->RestoreContext();
    contexts.Add(context);
  }
  for (int i = contexts.length() - 1; i >= 0; i--) {
    HandleScopeImplementer::instance()->SaveContext(contexts[i]);
  }
  writer_->PutC('C');
  writer_->PutC('[');
  writer_->PutInt(contexts.length());
  if (!contexts.is_empty()) {
    Object** start = reinterpret_cast<Object**>(&contexts.first());
    VisitPointers(start, start + contexts.length());
  }
  writer_->PutC(']');
}

void Serializer::PutEncodedAddress(Address addr) {
  writer_->PutC('P');
  writer_->PutAddress(addr);
}


Address Serializer::Encode(Object* o, bool* serialized) {
  *serialized = false;
  if (o->IsSmi()) {
    return reinterpret_cast<Address>(o);
  } else {
    HeapObject* obj = HeapObject::cast(o);
    if (IsVisited(obj)) {
      return GetSavedAddress(obj);
    } else {
      // First visit: serialize the object.
      *serialized = true;
      return PutObject(obj);
    }
  }
}


Address Serializer::PutObject(HeapObject* obj) {
  Map* map = obj->map();
  InstanceType type = map->instance_type();
  int size = obj->SizeFromMap(map);

  // Simulate the allocation of obj to predict where it will be
  // allocated during deserialization.
  Address addr = Allocate(obj).Encode();

  SaveAddress(obj, addr);

  if (type == CODE_TYPE) {
    LOG(CodeMoveEvent(obj->address(), addr));
  }

  // Write out the object prologue: type, size, and simulated address of obj.
  writer_->PutC('[');
  CHECK_EQ(0, static_cast<int>(size & kObjectAlignmentMask));
  writer_->PutInt(type);
  writer_->PutInt(size >> kObjectAlignmentBits);
  PutEncodedAddress(addr);  // encodes AllocationSpace

  // Visit all the pointers in the object other than the map. This
  // will recursively serialize any as-yet-unvisited objects.
  obj->Iterate(this);

  // Mark end of recursively embedded objects, start of object body.
  writer_->PutC('|');
  // Write out the raw contents of the object. No compression, but
  // fast to deserialize.
  writer_->PutBytes(obj->address(), size);
  // Update pointers and external references in the written object.
  ReferenceUpdater updater(obj, this);
  obj->Iterate(&updater);
  updater.Update(writer_->position() - size);

#ifdef DEBUG
  if (FLAG_debug_serialization) {
    // Write out the object epilogue to catch synchronization errors.
    PutEncodedAddress(addr);
    writer_->PutC(']');
  }
#endif

  objects_++;
  return addr;
}


RelativeAddress Serializer::Allocate(HeapObject* obj) {
  // Find out which AllocationSpace 'obj' is in.
  AllocationSpace s;
  bool found = false;
  for (int i = FIRST_SPACE; !found && i <= LAST_SPACE; i++) {
    s = static_cast<AllocationSpace>(i);
    found = Heap::InSpace(obj, s);
  }
  CHECK(found);
  int size = obj->Size();
  if (s == NEW_SPACE) {
    if (size > Heap::MaxObjectSizeInPagedSpace()) {
      s = LO_SPACE;
    } else {
      OldSpace* space = Heap::TargetSpace(obj);
      ASSERT(space == Heap::old_pointer_space() ||
             space == Heap::old_data_space());
      s = (space == Heap::old_pointer_space()) ?
          OLD_POINTER_SPACE :
          OLD_DATA_SPACE;
    }
  }
  GCTreatment gc_treatment = DataObject;
  if (obj->IsFixedArray()) gc_treatment = PointerObject;
  else if (obj->IsCode()) gc_treatment = CodeObject;
  return allocator_[s]->Allocate(size, gc_treatment);
}


//------------------------------------------------------------------------------
// Implementation of Deserializer


static const int kInitArraySize = 32;


Deserializer::Deserializer(const byte* str, int len)
  : reader_(str, len),
    map_pages_(kInitArraySize),
    cell_pages_(kInitArraySize),
    old_pointer_pages_(kInitArraySize),
    old_data_pages_(kInitArraySize),
    code_pages_(kInitArraySize),
    large_objects_(kInitArraySize),
    global_handles_(4) {
  root_ = true;
  roots_ = 0;
  objects_ = 0;
  reference_decoder_ = NULL;
#ifdef DEBUG
  expect_debug_information_ = false;
#endif
}


Deserializer::~Deserializer() {
  if (reference_decoder_) delete reference_decoder_;
}


void Deserializer::ExpectEncodedAddress(Address expected) {
  Address a = GetEncodedAddress();
  USE(a);
  ASSERT(a == expected);
}


#ifdef DEBUG
void Deserializer::Synchronize(const char* tag) {
  if (expect_debug_information_) {
    char buf[kMaxTagLength];
    reader_.ExpectC('S');
    int length = reader_.GetInt();
    ASSERT(length <= kMaxTagLength);
    reader_.GetBytes(reinterpret_cast<Address>(buf), length);
    ASSERT_EQ(strlen(tag), length);
    ASSERT(strncmp(tag, buf, length) == 0);
  }
}
#endif


class NoGlobalHandlesChecker : public ObjectVisitor {
 public:
  virtual void VisitPointers(Object** start, Object** end) {
    ASSERT(false);
  }
};


class GlobalHandleDestroyer : public ObjectVisitor {
  void VisitPointers(Object**start, Object**end) {
    while (start < end) {
      GlobalHandles::Destroy(start++);
    }
  }
};


void Deserializer::Deserialize() {
  // No global handles.
  NoGlobalHandlesChecker checker;
  GlobalHandles::IterateRoots(&checker);
  // No active threads.
  ASSERT_EQ(NULL, ThreadState::FirstInUse());
  // No active handles.
  ASSERT(HandleScopeImplementer::instance()->blocks()->is_empty());
  reference_decoder_ = new ExternalReferenceDecoder();
  // By setting linear allocation only, we forbid the use of free list
  // allocation which is not predicted by SimulatedAddress.
  GetHeader();
  Heap::IterateRoots(this);
  GetContextStack();
  // Any global handles that have been set up by deserialization are leaked
  // since noone is keeping track of them.  So we discard them now.
  GlobalHandleDestroyer destroyer;
  GlobalHandles::IterateRoots(&destroyer);
}


void Deserializer::VisitPointers(Object** start, Object** end) {
  bool root = root_;
  root_ = false;
  for (Object** p = start; p < end; ++p) {
    if (root) {
      roots_++;
      // Read the next object or pointer from the stream
      // pointer in the stream.
      int c = reader_.GetC();
      if (c == '[') {
        *p = GetObject();  // embedded object
      } else {
        ASSERT(c == 'P');  // pointer to previously serialized object
        *p = Resolve(reader_.GetAddress());
      }
    } else {
      // A pointer internal to a HeapObject that we've already
      // read: resolve it to a true address (or Smi)
      *p = Resolve(reinterpret_cast<Address>(*p));
    }
  }
  root_ = root;
}


void Deserializer::VisitCodeTarget(RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
  // On all platforms, the encoded code object address is only 32 bits.
  Address encoded_address = reinterpret_cast<Address>(Memory::uint32_at(
      reinterpret_cast<Address>(rinfo->target_object_address())));
  Code* target_object = reinterpret_cast<Code*>(Resolve(encoded_address));
  rinfo->set_target_address(target_object->instruction_start());
}


void Deserializer::VisitExternalReferences(Address* start, Address* end) {
  for (Address* p = start; p < end; ++p) {
    uint32_t code = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(*p));
    *p = reference_decoder_->Decode(code);
  }
}


void Deserializer::VisitRuntimeEntry(RelocInfo* rinfo) {
  uint32_t* pc = reinterpret_cast<uint32_t*>(rinfo->target_address_address());
  uint32_t encoding = *pc;
  Address target = reference_decoder_->Decode(encoding);
  rinfo->set_target_address(target);
}


void Deserializer::GetFlags() {
  reader_.ExpectC('F');
  int argc = reader_.GetInt() + 1;
  char** argv = NewArray<char*>(argc);
  reader_.ExpectC('[');
  for (int i = 1; i < argc; i++) {
    if (i > 1) reader_.ExpectC('|');
    argv[i] = reader_.GetString();
  }
  reader_.ExpectC(']');
  has_log_ = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp("--log_code", argv[i]) == 0) {
      has_log_ = true;
    } else if (strcmp("--nouse_ic", argv[i]) == 0) {
      FLAG_use_ic = false;
    } else if (strcmp("--debug_code", argv[i]) == 0) {
      FLAG_debug_code = true;
    } else if (strcmp("--nolazy", argv[i]) == 0) {
      FLAG_lazy = false;
    }
    DeleteArray(argv[i]);
  }

  DeleteArray(argv);
}


void Deserializer::GetLog() {
  if (has_log_) {
    reader_.ExpectC('L');
    char* snapshot_log = reader_.GetString();
#ifdef ENABLE_LOGGING_AND_PROFILING
    if (FLAG_log_code) {
      LOG(Preamble(snapshot_log));
    }
#endif
    DeleteArray(snapshot_log);
  }
}


static void InitPagedSpace(PagedSpace* space,
                           int capacity,
                           List<Page*>* page_list) {
  if (!space->EnsureCapacity(capacity)) {
    V8::FatalProcessOutOfMemory("InitPagedSpace");
  }
  PageIterator it(space, PageIterator::ALL_PAGES);
  while (it.has_next()) page_list->Add(it.next());
}


void Deserializer::GetHeader() {
  reader_.ExpectC('D');
#ifdef DEBUG
  expect_debug_information_ = reader_.GetC() == '1';
#else
  // In release mode, don't attempt to read a snapshot containing
  // synchronization tags.
  if (reader_.GetC() != '0') FATAL("Snapshot contains synchronization tags.");
#endif
#ifdef V8_NATIVE_REGEXP
  reader_.ExpectC('N');
#else  // Interpreted regexp.
  reader_.ExpectC('I');
#endif
  // Ensure sufficient capacity in paged memory spaces to avoid growth
  // during deserialization.
  reader_.ExpectC('S');
  reader_.ExpectC('[');
  InitPagedSpace(Heap::old_pointer_space(),
                 reader_.GetInt(),
                 &old_pointer_pages_);
  reader_.ExpectC('|');
  InitPagedSpace(Heap::old_data_space(), reader_.GetInt(), &old_data_pages_);
  reader_.ExpectC('|');
  InitPagedSpace(Heap::code_space(), reader_.GetInt(), &code_pages_);
  reader_.ExpectC('|');
  InitPagedSpace(Heap::map_space(), reader_.GetInt(), &map_pages_);
  reader_.ExpectC('|');
  InitPagedSpace(Heap::cell_space(), reader_.GetInt(), &cell_pages_);
  reader_.ExpectC(']');
  // Create placeholders for global handles later to be fill during
  // IterateRoots.
  reader_.ExpectC('G');
  reader_.ExpectC('[');
  int c = reader_.GetC();
  while (c != ']') {
    ASSERT(c == 'N');
    global_handles_.Add(GlobalHandles::Create(NULL).location());
    c = reader_.GetC();
  }
}


void Deserializer::GetGlobalHandleStack(List<Handle<Object> >* stack) {
  reader_.ExpectC('[');
  int length = reader_.GetInt();
  for (int i = 0; i < length; i++) {
    reader_.ExpectC('|');
    int gh_index = reader_.GetInt();
    stack->Add(global_handles_[gh_index]);
  }
  reader_.ExpectC(']');
}


void Deserializer::GetContextStack() {
  reader_.ExpectC('C');
  CHECK_EQ(reader_.GetC(), '[');
  int count = reader_.GetInt();
  List<Context*> entered_contexts(count);
  if (count > 0) {
    Object** start = reinterpret_cast<Object**>(&entered_contexts.first());
    VisitPointers(start, start + count);
  }
  reader_.ExpectC(']');
  for (int i = 0; i < count; i++) {
    HandleScopeImplementer::instance()->SaveContext(entered_contexts[i]);
  }
}


Address Deserializer::GetEncodedAddress() {
  reader_.ExpectC('P');
  return reader_.GetAddress();
}


Object* Deserializer::GetObject() {
  // Read the prologue: type, size and encoded address.
  InstanceType type = static_cast<InstanceType>(reader_.GetInt());
  int size = reader_.GetInt() << kObjectAlignmentBits;
  Address a = GetEncodedAddress();

  // Get a raw object of the right size in the right space.
  AllocationSpace space = GetSpace(a);
  Object* o;
  if (IsLargeExecutableObject(a)) {
    o = Heap::lo_space()->AllocateRawCode(size);
  } else if (IsLargeFixedArray(a)) {
    o = Heap::lo_space()->AllocateRawFixedArray(size);
  } else {
    AllocationSpace retry_space = (space == NEW_SPACE)
        ? Heap::TargetSpaceId(type)
        : space;
    o = Heap::AllocateRaw(size, space, retry_space);
  }
  ASSERT(!o->IsFailure());
  // Check that the simulation of heap allocation was correct.
  ASSERT(o == Resolve(a));

  // Read any recursively embedded objects.
  int c = reader_.GetC();
  while (c == '[') {
    GetObject();
    c = reader_.GetC();
  }
  ASSERT(c == '|');

  HeapObject* obj = reinterpret_cast<HeapObject*>(o);
  // Read the uninterpreted contents of the object after the map
  reader_.GetBytes(obj->address(), size);
#ifdef DEBUG
  if (expect_debug_information_) {
    // Read in the epilogue to check that we're still synchronized
    ExpectEncodedAddress(a);
    reader_.ExpectC(']');
  }
#endif

  // Resolve the encoded pointers we just read in.
  // Same as obj->Iterate(this), but doesn't rely on the map pointer being set.
  VisitPointer(reinterpret_cast<Object**>(obj->address()));
  obj->IterateBody(type, size, this);

  if (type == CODE_TYPE) {
    LOG(CodeMoveEvent(a, obj->address()));
  }
  objects_++;
  return o;
}


static inline Object* ResolvePaged(int page_index,
                                   int page_offset,
                                   PagedSpace* space,
                                   List<Page*>* page_list) {
  ASSERT(page_index < page_list->length());
  Address address = (*page_list)[page_index]->OffsetToAddress(page_offset);
  return HeapObject::FromAddress(address);
}


template<typename T>
void ConcatReversed(List<T>* target, const List<T>& source) {
  for (int i = source.length() - 1; i >= 0; i--) {
    target->Add(source[i]);
  }
}


Object* Deserializer::Resolve(Address encoded) {
  Object* o = reinterpret_cast<Object*>(encoded);
  if (o->IsSmi()) return o;

  // Encoded addresses of HeapObjects always have 'HeapObject' tags.
  ASSERT(o->IsHeapObject());
  switch (GetSpace(encoded)) {
    // For Map space and Old space, we cache the known Pages in map_pages,
    // old_pointer_pages and old_data_pages. Even though MapSpace keeps a list
    // of page addresses, we don't rely on it since GetObject uses AllocateRaw,
    // and that appears not to update the page list.
    case MAP_SPACE:
      return ResolvePaged(PageIndex(encoded), PageOffset(encoded),
                          Heap::map_space(), &map_pages_);
    case CELL_SPACE:
      return ResolvePaged(PageIndex(encoded), PageOffset(encoded),
                          Heap::cell_space(), &cell_pages_);
    case OLD_POINTER_SPACE:
      return ResolvePaged(PageIndex(encoded), PageOffset(encoded),
                          Heap::old_pointer_space(), &old_pointer_pages_);
    case OLD_DATA_SPACE:
      return ResolvePaged(PageIndex(encoded), PageOffset(encoded),
                          Heap::old_data_space(), &old_data_pages_);
    case CODE_SPACE:
      return ResolvePaged(PageIndex(encoded), PageOffset(encoded),
                          Heap::code_space(), &code_pages_);
    case NEW_SPACE:
      return HeapObject::FromAddress(Heap::NewSpaceStart() +
                                     NewSpaceOffset(encoded));
    case LO_SPACE:
      // Cache the known large_objects, allocated one per 'page'
      int index = LargeObjectIndex(encoded);
      if (index >= large_objects_.length()) {
        int new_object_count =
          Heap::lo_space()->PageCount() - large_objects_.length();
        List<Object*> new_objects(new_object_count);
        LargeObjectIterator it(Heap::lo_space());
        for (int i = 0; i < new_object_count; i++) {
          new_objects.Add(it.next());
        }
#ifdef DEBUG
        for (int i = large_objects_.length() - 1; i >= 0; i--) {
          ASSERT(it.next() == large_objects_[i]);
        }
#endif
        ConcatReversed(&large_objects_, new_objects);
        ASSERT(index < large_objects_.length());
      }
      return large_objects_[index];  // s.page_offset() is ignored.
  }
  UNREACHABLE();
  return NULL;
}


Deserializer2::Deserializer2(SnapshotByteSource* source)
    : source_(source),
      external_reference_decoder_(NULL) {
  for (int i = 0; i <= LAST_SPACE; i++) {
    fullness_[i] = 0;
  }
}


// This routine both allocates a new object, and also keeps
// track of where objects have been allocated so that we can
// fix back references when deserializing.
Address Deserializer2::Allocate(int space_index, int size) {
  HeapObject* new_object;
  int old_fullness = CurrentAllocationAddress(space_index);
  // When we start a new page we need to record its location.
  bool record_page = (old_fullness == 0);
  if (SpaceIsPaged(space_index)) {
    PagedSpace* space;
    switch (space_index) {
      case OLD_DATA_SPACE: space = Heap::old_data_space(); break;
      case OLD_POINTER_SPACE: space = Heap::old_pointer_space(); break;
      case MAP_SPACE: space = Heap::map_space(); break;
      case CODE_SPACE: space = Heap::code_space(); break;
      case CELL_SPACE: space = Heap::cell_space(); break;
      default: UNREACHABLE(); space = NULL; break;
    }
    ASSERT(size <= Page::kPageSize - Page::kObjectStartOffset);
    int current_page = old_fullness >> Page::kPageSizeBits;
    int new_fullness = old_fullness + size;
    int new_page = new_fullness >> Page::kPageSizeBits;
    // What is our new position within the current page.
    int intra_page_offset = new_fullness - current_page * Page::kPageSize;
    if (intra_page_offset > Page::kPageSize - Page::kObjectStartOffset) {
      // This object will not fit in a page and we have to move to the next.
      new_page = current_page + 1;
      old_fullness = new_page << Page::kPageSizeBits;
      new_fullness = old_fullness + size;
      record_page = true;
    }
    fullness_[space_index] = new_fullness;
    Object* new_allocation = space->AllocateRaw(size);
    new_object = HeapObject::cast(new_allocation);
    ASSERT(!new_object->IsFailure());
    ASSERT((reinterpret_cast<intptr_t>(new_object->address()) &
            Page::kPageAlignmentMask) ==
           (old_fullness & Page::kPageAlignmentMask) +
            Page::kObjectStartOffset);
  } else if (SpaceIsLarge(space_index)) {
    ASSERT(size > Page::kPageSize - Page::kObjectStartOffset);
    fullness_[LO_SPACE]++;
    LargeObjectSpace* lo_space = Heap::lo_space();
    Object* new_allocation;
    if (space_index == kLargeData) {
      new_allocation = lo_space->AllocateRaw(size);
    } else if (space_index == kLargeFixedArray) {
      new_allocation = lo_space->AllocateRawFixedArray(size);
    } else {
      ASSERT(space_index == kLargeCode);
      new_allocation = lo_space->AllocateRawCode(size);
    }
    ASSERT(!new_allocation->IsFailure());
    new_object = HeapObject::cast(new_allocation);
    record_page = true;
    // The page recording below records all large objects in the same space.
    space_index = LO_SPACE;
  } else {
    ASSERT(space_index == NEW_SPACE);
    Object* new_allocation = Heap::new_space()->AllocateRaw(size);
    fullness_[space_index] += size;
    ASSERT(!new_allocation->IsFailure());
    new_object = HeapObject::cast(new_allocation);
  }
  Address address = new_object->address();
  if (record_page) {
    pages_[space_index].Add(address);
  }
  return address;
}


// This returns the address of an object that has been described in the
// snapshot as being offset bytes back in a particular space.
HeapObject* Deserializer2::GetAddress(int space) {
  int offset = source_->GetInt();
  if (SpaceIsLarge(space)) {
    // Large spaces have one object per 'page'.
    return HeapObject::FromAddress(
        pages_[LO_SPACE][fullness_[LO_SPACE] - offset]);
  }
  offset <<= kObjectAlignmentBits;
  if (space == NEW_SPACE) {
    // New space has only one space - numbered 0.
    return HeapObject::FromAddress(
        pages_[space][0] + fullness_[space] - offset);
  }
  ASSERT(SpaceIsPaged(space));
  int virtual_address = fullness_[space] - offset;
  int page_of_pointee = (virtual_address) >> Page::kPageSizeBits;
  Address object_address = pages_[space][page_of_pointee] +
                           (virtual_address & Page::kPageAlignmentMask);
  return HeapObject::FromAddress(object_address);
}


void Deserializer2::Deserialize() {
  // Don't GC while deserializing - just expand the heap.
  AlwaysAllocateScope always_allocate;
  // Don't use the free lists while deserializing.
  LinearAllocationScope allocate_linearly;
  // No active threads.
  ASSERT_EQ(NULL, ThreadState::FirstInUse());
  // No active handles.
  ASSERT(HandleScopeImplementer::instance()->blocks()->is_empty());
  ASSERT(external_reference_decoder_ == NULL);
  external_reference_decoder_ = new ExternalReferenceDecoder();
  Heap::IterateRoots(this);
  ASSERT(source_->AtEOF());
  delete external_reference_decoder_;
  external_reference_decoder_ = NULL;
}


// This is called on the roots.  It is the driver of the deserialization
// process.
void Deserializer2::VisitPointers(Object** start, Object** end) {
  for (Object** current = start; current < end; current++) {
    DataType data = static_cast<DataType>(source_->Get());
    if (data == SMI_SERIALIZATION) {
      *current = Smi::FromInt(source_->GetInt() - kSmiBias);
    } else if (data == BACKREF_SERIALIZATION) {
      int space = source_->Get();
      *current = GetAddress(space);
    } else {
      ASSERT(data == OBJECT_SERIALIZATION);
      ReadObject(current);
    }
  }
}


// This routine writes the new object into the pointer provided and then
// returns true if the new object was in young space and false otherwise.
// The reason for this strange interface is that otherwise the object is
// written very late, which means the ByteArray map is not set up by the
// time we need to use it to mark the space at the end of a page free (by
// making it into a byte array).
bool Deserializer2::ReadObject(Object** write_back) {
  int space = source_->Get();
  int size = source_->GetInt() << kObjectAlignmentBits;
  Address address = Allocate(space, size);
  *write_back = HeapObject::FromAddress(address);
  Object** current = reinterpret_cast<Object**>(address);
  Object** limit = current + (size >> kPointerSizeLog2);
  while (current < limit) {
    DataType data = static_cast<DataType>(source_->Get());
    switch (data) {
      case SMI_SERIALIZATION:
        *current++ = Smi::FromInt(source_->GetInt() - kSmiBias);
        break;
      case RAW_DATA_SERIALIZATION: {
        int size = source_->GetInt();
        byte* raw_data_out = reinterpret_cast<byte*>(current);
        for (int j = 0; j < size; j++) {
          *raw_data_out++ = source_->Get();
        }
        current = reinterpret_cast<Object**>(raw_data_out);
        break;
      }
      case OBJECT_SERIALIZATION: {
        // Recurse to unpack an object that is forward-referenced from here.
        bool in_new_space = ReadObject(current);
        if (in_new_space && space != NEW_SPACE) {
          Heap::RecordWrite(address,
                            reinterpret_cast<Address>(current) - address);
        }
        current++;
        break;
      }
      case CODE_OBJECT_SERIALIZATION: {
        Object* new_code_object = NULL;
        ReadObject(&new_code_object);
        Code* code_object = reinterpret_cast<Code*>(new_code_object);
        // Setting a branch/call to another code object from code.
        Address location_of_branch_data = reinterpret_cast<Address>(current);
        Assembler::set_target_at(location_of_branch_data,
                                 code_object->instruction_start());
        location_of_branch_data += Assembler::kCallTargetSize;
        current = reinterpret_cast<Object**>(location_of_branch_data);
        break;
      }
      case BACKREF_SERIALIZATION: {
        // Write a backreference to an object we unpacked earlier.
        int backref_space = source_->Get();
        if (backref_space == NEW_SPACE && space != NEW_SPACE) {
          Heap::RecordWrite(address,
                            reinterpret_cast<Address>(current) - address);
        }
        *current++ = GetAddress(backref_space);
        break;
      }
      case CODE_BACKREF_SERIALIZATION: {
        int backref_space = source_->Get();
        // Can't use Code::cast because heap is not set up yet and assertions
        // will fail.
        Code* code_object = reinterpret_cast<Code*>(GetAddress(backref_space));
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
      default:
        UNREACHABLE();
    }
  }
  ASSERT(current == limit);
  return space == NEW_SPACE;
}


void SnapshotByteSink::PutInt(uintptr_t integer, const char* description) {
  const int max_shift = ((kPointerSize * kBitsPerByte) / 7) * 7;
  for (int shift = max_shift; shift > 0; shift -= 7) {
    if (integer >= 1u << shift) {
      Put(((integer >> shift) & 0x7f) | 0x80, "intpart");
    }
  }
  Put(integer & 0x7f, "intlastpart");
}

#ifdef DEBUG

void Deserializer2::Synchronize(const char* tag) {
  int data = source_->Get();
  // If this assert fails then that indicates that you have a mismatch between
  // the number of GC roots when serializing and deserializing.
  ASSERT(data == SYNCHRONIZE);
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


void Serializer2::Synchronize(const char* tag) {
  sink_->Put(SYNCHRONIZE, tag);
  int character;
  do {
    character = *tag++;
    sink_->Put(character, "tagcharacter");
  } while (character != 0);
}

#endif

Serializer2::Serializer2(SnapshotByteSink* sink)
    : sink_(sink),
      current_root_index_(0),
      external_reference_encoder_(NULL) {
  for (int i = 0; i <= LAST_SPACE; i++) {
    fullness_[i] = 0;
  }
}


void Serializer2::Serialize() {
  // No active threads.
  CHECK_EQ(NULL, ThreadState::FirstInUse());
  // No active or weak handles.
  CHECK(HandleScopeImplementer::instance()->blocks()->is_empty());
  CHECK_EQ(0, GlobalHandles::NumberOfWeakHandles());
  ASSERT(external_reference_encoder_ == NULL);
  external_reference_encoder_ = new ExternalReferenceEncoder();
  Heap::IterateRoots(this);
  delete external_reference_encoder_;
  external_reference_encoder_ = NULL;
}


void Serializer2::VisitPointers(Object** start, Object** end) {
  for (Object** current = start; current < end; current++) {
    SerializeObject(*current, TAGGED_REPRESENTATION);
  }
}


void Serializer2::SerializeObject(
    Object* o,
    ReferenceRepresentation reference_representation) {
  if (o->IsHeapObject()) {
    HeapObject* heap_object = HeapObject::cast(o);
    MapWord map_word = heap_object->map_word();
    if (map_word.IsSerializationAddress()) {
      int space = SpaceOfAlreadySerializedObject(heap_object);
      int offset =
          CurrentAllocationAddress(space) - map_word.ToSerializationAddress();
      // If we are actually dealing with real offsets (and not a numbering of
      // all objects) then we should shift out the bits that are always 0.
      if (!SpaceIsLarge(space)) offset >>= kObjectAlignmentBits;
      if (reference_representation == CODE_TARGET_REPRESENTATION) {
        sink_->Put(CODE_BACKREF_SERIALIZATION, "BackRefCodeSerialization");
      } else {
        ASSERT(reference_representation == TAGGED_REPRESENTATION);
        sink_->Put(BACKREF_SERIALIZATION, "BackRefSerialization");
      }
      sink_->Put(space, "space");
      sink_->PutInt(offset, "offset");
    } else {
      // Object has not yet been serialized.  Serialize it here.
      ObjectSerializer serializer(this,
                                  heap_object,
                                  sink_,
                                  reference_representation);
      serializer.Serialize();
    }
  } else {
    // Serialize a Smi.
    unsigned int value = Smi::cast(o)->value() + kSmiBias;
    sink_->Put(SMI_SERIALIZATION, "SmiSerialization");
    sink_->PutInt(value, "smi");
  }
}


void Serializer2::ObjectSerializer::Serialize() {
  int space = Serializer2::SpaceOfObject(object_);
  int size = object_->Size();

  if (reference_representation_ == TAGGED_REPRESENTATION) {
    sink_->Put(OBJECT_SERIALIZATION, "ObjectSerialization");
  } else {
    ASSERT(reference_representation_ == CODE_TARGET_REPRESENTATION);
    sink_->Put(CODE_OBJECT_SERIALIZATION, "ObjectSerialization");
  }
  sink_->Put(space, "space");
  sink_->PutInt(size >> kObjectAlignmentBits, "Size in words");

  // Get the map before overwriting it.
  Map* map = object_->map();
  // Mark this object as already serialized.
  object_->set_map_word(
      MapWord::FromSerializationAddress(serializer_->Allocate(space, size)));

  // Serialize the map (first word of the object).
  serializer_->SerializeObject(map, TAGGED_REPRESENTATION);

  // Serialize the rest of the object.
  ASSERT(bytes_processed_so_far_ == 0);
  bytes_processed_so_far_ = kPointerSize;
  object_->IterateBody(map->instance_type(), size, this);
  OutputRawData(object_->address() + size);
}


void Serializer2::ObjectSerializer::VisitPointers(Object** start,
                                                  Object** end) {
  Address pointers_start = reinterpret_cast<Address>(start);
  OutputRawData(pointers_start);

  for (Object** current = start; current < end; current++) {
    serializer_->SerializeObject(*current, TAGGED_REPRESENTATION);
  }
  bytes_processed_so_far_ += (end - start) * kPointerSize;
}


void Serializer2::ObjectSerializer::VisitExternalReferences(Address* start,
                                                            Address* end) {
  Address references_start = reinterpret_cast<Address>(start);
  OutputRawData(references_start);

  for (Address* current = start; current < end; current++) {
    sink_->Put(EXTERNAL_REFERENCE_SERIALIZATION, "External reference");
    int reference_id = serializer_->EncodeExternalReference(*current);
    sink_->PutInt(reference_id, "reference id");
  }
  bytes_processed_so_far_ += (end - start) * kPointerSize;
}


void Serializer2::ObjectSerializer::VisitCodeTarget(RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
  Address target_start = rinfo->target_address_address();
  OutputRawData(target_start);
  Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  serializer_->SerializeObject(target, CODE_TARGET_REPRESENTATION);
  bytes_processed_so_far_ += Assembler::kCallTargetSize;
}


void Serializer2::ObjectSerializer::OutputRawData(Address up_to) {
  Address object_start = object_->address();
  int up_to_offset = up_to - object_start;
  int skipped = up_to_offset - bytes_processed_so_far_;
  // This assert will fail if the reloc info gives us the target_address_address
  // locations in a non-ascending order.  Luckily that doesn't happen.
  ASSERT(skipped >= 0);
  if (skipped != 0) {
    sink_->Put(RAW_DATA_SERIALIZATION, "raw data");
    sink_->PutInt(skipped, "length");
    for (int i = 0; i < skipped; i++) {
      unsigned int data = object_start[bytes_processed_so_far_ + i];
      sink_->Put(data, "byte");
    }
  }
  bytes_processed_so_far_ += skipped;
}


int Serializer2::SpaceOfObject(HeapObject* object) {
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


int Serializer2::SpaceOfAlreadySerializedObject(HeapObject* object) {
  for (int i = FIRST_SPACE; i <= LAST_SPACE; i++) {
    AllocationSpace s = static_cast<AllocationSpace>(i);
    if (Heap::InSpace(object, s)) {
      return i;
    }
  }
  UNREACHABLE();
  return 0;
}


int Serializer2::Allocate(int space, int size) {
  ASSERT(space >= 0 && space < kNumberOfSpaces);
  if (SpaceIsLarge(space)) {
    // In large object space we merely number the objects instead of trying to
    // determine some sort of address.
    return fullness_[LO_SPACE]++;
  }
  if (SpaceIsPaged(space)) {
    // Paged spaces are a little special.  We encode their addresses as if the
    // pages were all contiguous and each page were filled up in the range
    // 0 - Page::kObjectAreaSize.  In practice the pages may not be contiguous
    // and allocation does not start at offset 0 in the page, but this scheme
    // means the deserializer can get the page number quickly by shifting the
    // serialized address.
    ASSERT(IsPowerOf2(Page::kPageSize));
    int used_in_this_page = (fullness_[space] & (Page::kPageSize - 1));
    ASSERT(size <= Page::kObjectAreaSize);
    if (used_in_this_page + size > Page::kObjectAreaSize) {
      fullness_[space] = RoundUp(fullness_[space], Page::kPageSize);
    }
  }
  int allocation_address = fullness_[space];
  fullness_[space] = allocation_address + size;
  return allocation_address;
}


} }  // namespace v8::internal
