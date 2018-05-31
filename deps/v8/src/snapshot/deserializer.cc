// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/deserializer.h"

#include "src/assembler-inl.h"
#include "src/isolate.h"
#include "src/objects/hash-table.h"
#include "src/objects/maybe-object.h"
#include "src/objects/string.h"
#include "src/snapshot/builtin-deserializer-allocator.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

template <class AllocatorT>
void Deserializer<AllocatorT>::Initialize(Isolate* isolate) {
  DCHECK_NULL(isolate_);
  DCHECK_NOT_NULL(isolate);
  isolate_ = isolate;
  DCHECK_NULL(external_reference_table_);
  external_reference_table_ = isolate->heap()->external_reference_table();
#ifdef DEBUG
  // Count the number of external references registered through the API.
  num_api_references_ = 0;
  if (isolate_->api_external_references() != nullptr) {
    while (isolate_->api_external_references()[num_api_references_] != 0) {
      num_api_references_++;
    }
  }
#endif  // DEBUG
  CHECK_EQ(magic_number_,
           SerializedData::ComputeMagicNumber(external_reference_table_));
}

template <class AllocatorT>
bool Deserializer<AllocatorT>::IsLazyDeserializationEnabled() const {
  return FLAG_lazy_deserialization && !isolate()->serializer_enabled();
}

template <class AllocatorT>
void Deserializer<AllocatorT>::Rehash() {
  DCHECK(can_rehash() || deserializing_user_code());
  for (const auto& item : to_rehash_) item->RehashBasedOnMap();
}

template <class AllocatorT>
Deserializer<AllocatorT>::~Deserializer() {
#ifdef DEBUG
  // Do not perform checks if we aborted deserialization.
  if (source_.position() == 0) return;
  // Check that we only have padding bytes remaining.
  while (source_.HasMore()) DCHECK_EQ(kNop, source_.Get());
  // Check that we've fully used all reserved space.
  DCHECK(allocator()->ReservationsAreFullyUsed());
#endif  // DEBUG
}

// This is called on the roots.  It is the driver of the deserialization
// process.  It is also called on the body of each function.
template <class AllocatorT>
void Deserializer<AllocatorT>::VisitRootPointers(Root root,
                                                 const char* description,
                                                 Object** start, Object** end) {
  // Builtins and bytecode handlers are deserialized in a separate pass by the
  // BuiltinDeserializer.
  if (root == Root::kBuiltins || root == Root::kDispatchTable) return;

  // The space must be new space.  Any other space would cause ReadChunk to try
  // to update the remembered using nullptr as the address.
  ReadData(reinterpret_cast<MaybeObject**>(start),
           reinterpret_cast<MaybeObject**>(end), NEW_SPACE, nullptr);
}

template <class AllocatorT>
void Deserializer<AllocatorT>::Synchronize(
    VisitorSynchronization::SyncTag tag) {
  static const byte expected = kSynchronize;
  CHECK_EQ(expected, source_.Get());
}

template <class AllocatorT>
void Deserializer<AllocatorT>::DeserializeDeferredObjects() {
  for (int code = source_.Get(); code != kSynchronize; code = source_.Get()) {
    switch (code) {
      case kAlignmentPrefix:
      case kAlignmentPrefix + 1:
      case kAlignmentPrefix + 2: {
        int alignment = code - (SerializerDeserializer::kAlignmentPrefix - 1);
        allocator()->SetAlignment(static_cast<AllocationAlignment>(alignment));
        break;
      }
      default: {
        int space = code & kSpaceMask;
        DCHECK_LE(space, kNumberOfSpaces);
        DCHECK_EQ(code - space, kNewObject);
        HeapObject* object = GetBackReferencedObject(space);
        int size = source_.GetInt() << kPointerSizeLog2;
        Address obj_address = object->address();
        MaybeObject** start =
            reinterpret_cast<MaybeObject**>(obj_address + kPointerSize);
        MaybeObject** end = reinterpret_cast<MaybeObject**>(obj_address + size);
        bool filled = ReadData(start, end, space, obj_address);
        CHECK(filled);
        DCHECK(CanBeDeferred(object));
        PostProcessNewObject(object, space);
      }
    }
  }
}

StringTableInsertionKey::StringTableInsertionKey(String* string)
    : StringTableKey(ComputeHashField(string)), string_(string) {
  DCHECK(string->IsInternalizedString());
}

bool StringTableInsertionKey::IsMatch(Object* string) {
  // We know that all entries in a hash table had their hash keys created.
  // Use that knowledge to have fast failure.
  if (Hash() != String::cast(string)->Hash()) return false;
  // We want to compare the content of two internalized strings here.
  return string_->SlowEquals(String::cast(string));
}

Handle<String> StringTableInsertionKey::AsHandle(Isolate* isolate) {
  return handle(string_, isolate);
}

uint32_t StringTableInsertionKey::ComputeHashField(String* string) {
  // Make sure hash_field() is computed.
  string->Hash();
  return string->hash_field();
}

template <class AllocatorT>
HeapObject* Deserializer<AllocatorT>::PostProcessNewObject(HeapObject* obj,
                                                           int space) {
  if ((FLAG_rehash_snapshot && can_rehash_) || deserializing_user_code()) {
    if (obj->IsString()) {
      // Uninitialize hash field as we need to recompute the hash.
      String* string = String::cast(obj);
      string->set_hash_field(String::kEmptyHashField);
    } else if (obj->NeedsRehashing()) {
      to_rehash_.push_back(obj);
    }
  }

  if (deserializing_user_code()) {
    if (obj->IsString()) {
      String* string = String::cast(obj);
      if (string->IsInternalizedString()) {
        // Canonicalize the internalized string. If it already exists in the
        // string table, set it to forward to the existing one.
        StringTableInsertionKey key(string);
        String* canonical =
            StringTable::ForwardStringIfExists(isolate_, &key, string);

        if (canonical != nullptr) return canonical;

        new_internalized_strings_.push_back(handle(string));
        return string;
      }
    } else if (obj->IsScript()) {
      new_scripts_.push_back(handle(Script::cast(obj)));
    } else {
      DCHECK(CanBeDeferred(obj));
    }
  }
  if (obj->IsAllocationSite()) {
    // Allocation sites are present in the snapshot, and must be linked into
    // a list at deserialization time.
    AllocationSite* site = AllocationSite::cast(obj);
    // TODO(mvstanton): consider treating the heap()->allocation_sites_list()
    // as a (weak) root. If this root is relocated correctly, this becomes
    // unnecessary.
    if (isolate_->heap()->allocation_sites_list() == Smi::kZero) {
      site->set_weak_next(isolate_->heap()->undefined_value());
    } else {
      site->set_weak_next(isolate_->heap()->allocation_sites_list());
    }
    isolate_->heap()->set_allocation_sites_list(site);
  } else if (obj->IsCode()) {
    // We flush all code pages after deserializing the startup snapshot. In that
    // case, we only need to remember code objects in the large object space.
    // When deserializing user code, remember each individual code object.
    if (deserializing_user_code() || space == LO_SPACE) {
      new_code_objects_.push_back(Code::cast(obj));
    }
  } else if (obj->IsAccessorInfo()) {
    if (isolate_->external_reference_redirector()) {
      accessor_infos_.push_back(AccessorInfo::cast(obj));
    }
  } else if (obj->IsCallHandlerInfo()) {
    if (isolate_->external_reference_redirector()) {
      call_handler_infos_.push_back(CallHandlerInfo::cast(obj));
    }
  } else if (obj->IsExternalString()) {
    if (obj->map() == isolate_->heap()->native_source_string_map()) {
      ExternalOneByteString* string = ExternalOneByteString::cast(obj);
      DCHECK(string->is_short());
      string->set_resource(
          NativesExternalStringResource::DecodeForDeserialization(
              string->resource()));
    } else {
      ExternalString* string = ExternalString::cast(obj);
      uint32_t index = string->resource_as_uint32();
      Address address =
          reinterpret_cast<Address>(isolate_->api_external_references()[index]);
      string->set_address_as_resource(address);
    }
    isolate_->heap()->RegisterExternalString(String::cast(obj));
  } else if (obj->IsJSTypedArray()) {
    JSTypedArray* typed_array = JSTypedArray::cast(obj);
    CHECK(typed_array->byte_offset()->IsSmi());
    int32_t byte_offset = NumberToInt32(typed_array->byte_offset());
    if (byte_offset > 0) {
      FixedTypedArrayBase* elements =
          FixedTypedArrayBase::cast(typed_array->elements());
      // Must be off-heap layout.
      DCHECK(!typed_array->is_on_heap());

      void* pointer_with_offset = reinterpret_cast<void*>(
          reinterpret_cast<intptr_t>(elements->external_pointer()) +
          byte_offset);
      elements->set_external_pointer(pointer_with_offset);
    }
  } else if (obj->IsJSArrayBuffer()) {
    JSArrayBuffer* buffer = JSArrayBuffer::cast(obj);
    // Only fixup for the off-heap case.
    if (buffer->backing_store() != nullptr) {
      Smi* store_index = reinterpret_cast<Smi*>(buffer->backing_store());
      void* backing_store = off_heap_backing_stores_[store_index->value()];

      buffer->set_backing_store(backing_store);
      isolate_->heap()->RegisterNewArrayBuffer(buffer);
    }
  } else if (obj->IsFixedTypedArrayBase()) {
    FixedTypedArrayBase* fta = FixedTypedArrayBase::cast(obj);
    // Only fixup for the off-heap case.
    if (fta->base_pointer() == nullptr) {
      Smi* store_index = reinterpret_cast<Smi*>(fta->external_pointer());
      void* backing_store = off_heap_backing_stores_[store_index->value()];
      fta->set_external_pointer(backing_store);
    }
  } else if (obj->IsBytecodeArray()) {
    // TODO(mythria): Remove these once we store the default values for these
    // fields in the serializer.
    BytecodeArray* bytecode_array = BytecodeArray::cast(obj);
    bytecode_array->set_interrupt_budget(
        interpreter::Interpreter::InterruptBudget());
    bytecode_array->set_osr_loop_nesting_level(0);
  }

  // Check alignment.
  DCHECK_EQ(0, Heap::GetFillToAlign(obj->address(),
                                    HeapObject::RequiredAlignment(obj->map())));
  return obj;
}

template <class AllocatorT>
int Deserializer<AllocatorT>::MaybeReplaceWithDeserializeLazy(int builtin_id) {
  DCHECK(Builtins::IsBuiltinId(builtin_id));
  return IsLazyDeserializationEnabled() && Builtins::IsLazy(builtin_id)
             ? Builtins::kDeserializeLazy
             : builtin_id;
}

template <class AllocatorT>
HeapObject* Deserializer<AllocatorT>::GetBackReferencedObject(int space) {
  HeapObject* obj;
  SerializerReference back_reference =
      SerializerReference::FromBitfield(source_.GetInt());

  switch (space) {
    case LO_SPACE:
      obj = allocator()->GetLargeObject(back_reference.large_object_index());
      break;
    case MAP_SPACE:
      obj = allocator()->GetMap(back_reference.map_index());
      break;
    case RO_SPACE:
      if (isolate()->heap()->deserialization_complete()) {
        PagedSpace* read_only_space = isolate()->heap()->read_only_space();
        Page* page = read_only_space->FirstPage();
        for (uint32_t i = 0; i < back_reference.chunk_index(); ++i) {
          page = page->next_page();
        }
        Address address = page->OffsetToAddress(back_reference.chunk_offset());
        obj = HeapObject::FromAddress(address);
        break;
      }
      V8_FALLTHROUGH;
    default:
      obj = allocator()->GetObject(static_cast<AllocationSpace>(space),
                                   back_reference.chunk_index(),
                                   back_reference.chunk_offset());
      break;
  }

  if (deserializing_user_code() && obj->IsThinString()) {
    obj = ThinString::cast(obj)->actual();
  }

  hot_objects_.Add(obj);
  DCHECK(!HasWeakHeapObjectTag(obj));
  return obj;
}

// This routine writes the new object into the pointer provided and then
// returns true if the new object was in young space and false otherwise.
// The reason for this strange interface is that otherwise the object is
// written very late, which means the FreeSpace map is not set up by the
// time we need to use it to mark the space at the end of a page free.
template <class AllocatorT>
void Deserializer<AllocatorT>::ReadObject(
    int space_number, MaybeObject** write_back,
    HeapObjectReferenceType reference_type) {
  const int size = source_.GetInt() << kObjectAlignmentBits;

  Address address =
      allocator()->Allocate(static_cast<AllocationSpace>(space_number), size);
  HeapObject* obj = HeapObject::FromAddress(address);

  isolate_->heap()->OnAllocationEvent(obj, size);
  MaybeObject** current = reinterpret_cast<MaybeObject**>(address);
  MaybeObject** limit = current + (size >> kPointerSizeLog2);

  if (ReadData(current, limit, space_number, address)) {
    // Only post process if object content has not been deferred.
    obj = PostProcessNewObject(obj, space_number);
  }

  MaybeObject* write_back_obj =
      reference_type == HeapObjectReferenceType::STRONG
          ? HeapObjectReference::Strong(obj)
          : HeapObjectReference::Weak(obj);
  UnalignedCopy(write_back, &write_back_obj);
#ifdef DEBUG
  if (obj->IsCode()) {
    DCHECK(space_number == CODE_SPACE || space_number == LO_SPACE);
  } else {
    DCHECK(space_number != CODE_SPACE);
  }
#endif  // DEBUG
}

template <class AllocatorT>
Object* Deserializer<AllocatorT>::ReadDataSingle() {
  MaybeObject* o;
  MaybeObject** start = &o;
  MaybeObject** end = start + 1;
  int source_space = NEW_SPACE;
  Address current_object = nullptr;

  CHECK(ReadData(start, end, source_space, current_object));
  HeapObject* heap_object;
  bool success = o->ToStrongHeapObject(&heap_object);
  DCHECK(success);
  USE(success);
  return heap_object;
}

static void NoExternalReferencesCallback() {
  // The following check will trigger if a function or object template
  // with references to native functions have been deserialized from
  // snapshot, but no actual external references were provided when the
  // isolate was created.
  CHECK_WITH_MSG(false, "No external references provided via API");
}

template <class AllocatorT>
bool Deserializer<AllocatorT>::ReadData(MaybeObject** current,
                                        MaybeObject** limit, int source_space,
                                        Address current_object_address) {
  Isolate* const isolate = isolate_;
  // Write barrier support costs around 1% in startup time.  In fact there
  // are no new space objects in current boot snapshots, so it's not needed,
  // but that may change.
  bool write_barrier_needed =
      (current_object_address != nullptr && source_space != NEW_SPACE &&
       source_space != CODE_SPACE);
  while (current < limit) {
    byte data = source_.Get();
    switch (data) {
#define CASE_STATEMENT(where, how, within, space_number) \
  case where + how + within + space_number:              \
    STATIC_ASSERT((where & ~kWhereMask) == 0);           \
    STATIC_ASSERT((how & ~kHowToCodeMask) == 0);         \
    STATIC_ASSERT((within & ~kWhereToPointMask) == 0);   \
    STATIC_ASSERT((space_number & ~kSpaceMask) == 0);

#define CASE_BODY(where, how, within, space_number_if_any)                   \
  current = ReadDataCase<where, how, within, space_number_if_any>(           \
      isolate, current, current_object_address, data, write_barrier_needed); \
  break;

// This generates a case and a body for the new space (which has to do extra
// write barrier handling) and handles the other spaces with fall-through cases
// and one body.
#define ALL_SPACES(where, how, within)           \
  CASE_STATEMENT(where, how, within, NEW_SPACE)  \
  CASE_BODY(where, how, within, NEW_SPACE)       \
  CASE_STATEMENT(where, how, within, OLD_SPACE)  \
  V8_FALLTHROUGH;                                \
  CASE_STATEMENT(where, how, within, CODE_SPACE) \
  V8_FALLTHROUGH;                                \
  CASE_STATEMENT(where, how, within, MAP_SPACE)  \
  V8_FALLTHROUGH;                                \
  CASE_STATEMENT(where, how, within, LO_SPACE)   \
  V8_FALLTHROUGH;                                \
  CASE_STATEMENT(where, how, within, RO_SPACE)   \
  CASE_BODY(where, how, within, kAnyOldSpace)

#define FOUR_CASES(byte_code) \
  case byte_code:             \
  case byte_code + 1:         \
  case byte_code + 2:         \
  case byte_code + 3:

#define SIXTEEN_CASES(byte_code) \
  FOUR_CASES(byte_code)          \
  FOUR_CASES(byte_code + 4)      \
  FOUR_CASES(byte_code + 8)      \
  FOUR_CASES(byte_code + 12)

#define SINGLE_CASE(where, how, within, space) \
  CASE_STATEMENT(where, how, within, space)    \
  CASE_BODY(where, how, within, space)

      // Deserialize a new object and write a pointer to it to the current
      // object.
      ALL_SPACES(kNewObject, kPlain, kStartOfObject)
      // Deserialize a new code object and write a pointer to its first
      // instruction to the current code object.
      ALL_SPACES(kNewObject, kFromCode, kInnerPointer)
      // Find a recently deserialized object using its offset from the current
      // allocation point and write a pointer to it to the current object.
      ALL_SPACES(kBackref, kPlain, kStartOfObject)
      ALL_SPACES(kBackrefWithSkip, kPlain, kStartOfObject)
#if V8_CODE_EMBEDS_OBJECT_POINTER
      // Deserialize a new object from pointer found in code and write
      // a pointer to it to the current object. Required only for MIPS, PPC, ARM
      // or S390 with embedded constant pool, and omitted on the other
      // architectures because it is fully unrolled and would cause bloat.
      ALL_SPACES(kNewObject, kFromCode, kStartOfObject)
      // Find a recently deserialized code object using its offset from the
      // current allocation point and write a pointer to it to the current
      // object. Required only for MIPS, PPC, ARM or S390 with embedded
      // constant pool.
      ALL_SPACES(kBackref, kFromCode, kStartOfObject)
      ALL_SPACES(kBackrefWithSkip, kFromCode, kStartOfObject)
#endif
      // Find a recently deserialized code object using its offset from the
      // current allocation point and write a pointer to its first instruction
      // to the current code object or the instruction pointer in a function
      // object.
      ALL_SPACES(kBackref, kFromCode, kInnerPointer)
      ALL_SPACES(kBackrefWithSkip, kFromCode, kInnerPointer)
      // Find an object in the roots array and write a pointer to it to the
      // current object.
      SINGLE_CASE(kRootArray, kPlain, kStartOfObject, 0)
#if V8_CODE_EMBEDS_OBJECT_POINTER
      // Find an object in the roots array and write a pointer to it to in code.
      SINGLE_CASE(kRootArray, kFromCode, kStartOfObject, 0)
#endif
      // Find an object in the partial snapshots cache and write a pointer to it
      // to the current object.
      SINGLE_CASE(kPartialSnapshotCache, kPlain, kStartOfObject, 0)
      SINGLE_CASE(kPartialSnapshotCache, kFromCode, kStartOfObject, 0)
      SINGLE_CASE(kPartialSnapshotCache, kFromCode, kInnerPointer, 0)
      // Find an object in the attached references and write a pointer to it to
      // the current object.
      SINGLE_CASE(kAttachedReference, kPlain, kStartOfObject, 0)
      SINGLE_CASE(kAttachedReference, kFromCode, kStartOfObject, 0)
      SINGLE_CASE(kAttachedReference, kFromCode, kInnerPointer, 0)
      // Find a builtin and write a pointer to it to the current object.
      SINGLE_CASE(kBuiltin, kPlain, kStartOfObject, 0)
      SINGLE_CASE(kBuiltin, kFromCode, kStartOfObject, 0)
      SINGLE_CASE(kBuiltin, kFromCode, kInnerPointer, 0)

#undef CASE_STATEMENT
#undef CASE_BODY
#undef ALL_SPACES

      case kSkip: {
        int size = source_.GetInt();
        current = reinterpret_cast<MaybeObject**>(
            reinterpret_cast<intptr_t>(current) + size);
        break;
      }

      // Find an external reference and write a pointer to it to the current
      // object.
      case kExternalReference + kPlain + kStartOfObject:
        current = reinterpret_cast<MaybeObject**>(ReadExternalReferenceCase(
            kPlain, isolate, reinterpret_cast<void**>(current),
            current_object_address));
        break;
      // Find an external reference and write a pointer to it in the current
      // code object.
      case kExternalReference + kFromCode + kStartOfObject:
        current = reinterpret_cast<MaybeObject**>(ReadExternalReferenceCase(
            kFromCode, isolate, reinterpret_cast<void**>(current),
            current_object_address));
        break;

      case kInternalReferenceEncoded:
      case kInternalReference: {
        // Internal reference address is not encoded via skip, but by offset
        // from code entry.
        int pc_offset = source_.GetInt();
        int target_offset = source_.GetInt();
        Code* code =
            Code::cast(HeapObject::FromAddress(current_object_address));
        DCHECK(0 <= pc_offset && pc_offset <= code->raw_instruction_size());
        DCHECK(0 <= target_offset &&
               target_offset <= code->raw_instruction_size());
        Address pc = code->entry() + pc_offset;
        Address target = code->entry() + target_offset;
        Assembler::deserialization_set_target_internal_reference_at(
            pc, target,
            data == kInternalReference ? RelocInfo::INTERNAL_REFERENCE
                                       : RelocInfo::INTERNAL_REFERENCE_ENCODED);
        break;
      }

      case kOffHeapTarget: {
#ifdef V8_EMBEDDED_BUILTINS
        int skip = source_.GetInt();
        int builtin_index = source_.GetInt();
        DCHECK(Builtins::IsBuiltinId(builtin_index));

        current = reinterpret_cast<MaybeObject**>(
            reinterpret_cast<Address>(current) + skip);

        CHECK_NOT_NULL(isolate->embedded_blob());
        EmbeddedData d = EmbeddedData::FromBlob();
        const uint8_t* address = d.InstructionStartOfBuiltin(builtin_index);
        CHECK_NOT_NULL(address);

        if (RelocInfo::OffHeapTargetIsCodedSpecially()) {
          Address location_of_branch_data = reinterpret_cast<Address>(current);
          Assembler::deserialization_set_special_target_at(
              location_of_branch_data,
              Code::cast(HeapObject::FromAddress(current_object_address)),
              const_cast<Address>(address));
          location_of_branch_data += Assembler::kSpecialTargetSize;
          current = reinterpret_cast<MaybeObject**>(location_of_branch_data);
        } else {
          MaybeObject* o =
              reinterpret_cast<MaybeObject*>(const_cast<uint8_t*>(address));
          UnalignedCopy(current, &o);
          current++;
        }
#else
        UNREACHABLE();
#endif
        break;
      }

      case kNop:
        break;

      case kNextChunk: {
        int space = source_.Get();
        allocator()->MoveToNextChunk(static_cast<AllocationSpace>(space));
        break;
      }

      case kDeferred: {
        // Deferred can only occur right after the heap object header.
        DCHECK_EQ(current, reinterpret_cast<MaybeObject**>(
                               current_object_address + kPointerSize));
        HeapObject* obj = HeapObject::FromAddress(current_object_address);
        // If the deferred object is a map, its instance type may be used
        // during deserialization. Initialize it with a temporary value.
        if (obj->IsMap()) Map::cast(obj)->set_instance_type(FILLER_TYPE);
        current = limit;
        return false;
      }

      case kSynchronize:
        // If we get here then that indicates that you have a mismatch between
        // the number of GC roots when serializing and deserializing.
        UNREACHABLE();

      // Deserialize raw data of variable length.
      case kVariableRawData: {
        int size_in_bytes = source_.GetInt();
        byte* raw_data_out = reinterpret_cast<byte*>(current);
        source_.CopyRaw(raw_data_out, size_in_bytes);
        current = reinterpret_cast<MaybeObject**>(
            reinterpret_cast<intptr_t>(current) + size_in_bytes);
        break;
      }

      // Deserialize raw code directly into the body of the code object.
      // Do not move current.
      case kVariableRawCode: {
        int size_in_bytes = source_.GetInt();
        source_.CopyRaw(current_object_address + Code::kDataStart,
                        size_in_bytes);
        break;
      }

      case kVariableRepeat: {
        int repeats = source_.GetInt();
        MaybeObject* object = current[-1];
        DCHECK(!isolate->heap()->InNewSpace(object));
        DCHECK(!allocator()->next_reference_is_weak());
        for (int i = 0; i < repeats; i++) UnalignedCopy(current++, &object);
        break;
      }

      case kOffHeapBackingStore: {
        int byte_length = source_.GetInt();
        byte* backing_store = static_cast<byte*>(
            isolate->array_buffer_allocator()->AllocateUninitialized(
                byte_length));
        CHECK_NOT_NULL(backing_store);
        source_.CopyRaw(backing_store, byte_length);
        off_heap_backing_stores_.push_back(backing_store);
        break;
      }

      case kApiReference: {
        int skip = source_.GetInt();
        current = reinterpret_cast<MaybeObject**>(
            reinterpret_cast<Address>(current) + skip);
        uint32_t reference_id = static_cast<uint32_t>(source_.GetInt());
        Address address;
        if (isolate->api_external_references()) {
          DCHECK_WITH_MSG(
              reference_id < num_api_references_,
              "too few external references provided through the API");
          address = reinterpret_cast<Address>(
              isolate->api_external_references()[reference_id]);
        } else {
          address = reinterpret_cast<Address>(NoExternalReferencesCallback);
        }
        memcpy(current, &address, kPointerSize);
        current++;
        break;
      }

      case kWeakPrefix:
        DCHECK(!allocator()->next_reference_is_weak());
        allocator()->set_next_reference_is_weak(true);
        break;

      case kAlignmentPrefix:
      case kAlignmentPrefix + 1:
      case kAlignmentPrefix + 2: {
        int alignment = data - (SerializerDeserializer::kAlignmentPrefix - 1);
        allocator()->SetAlignment(static_cast<AllocationAlignment>(alignment));
        break;
      }

      STATIC_ASSERT(kNumberOfRootArrayConstants == Heap::kOldSpaceRoots);
      STATIC_ASSERT(kNumberOfRootArrayConstants == 32);
      SIXTEEN_CASES(kRootArrayConstantsWithSkip)
      SIXTEEN_CASES(kRootArrayConstantsWithSkip + 16) {
        int skip = source_.GetInt();
        current = reinterpret_cast<MaybeObject**>(
            reinterpret_cast<intptr_t>(current) + skip);
        V8_FALLTHROUGH;
      }

      SIXTEEN_CASES(kRootArrayConstants)
      SIXTEEN_CASES(kRootArrayConstants + 16) {
        int id = data & kRootArrayConstantsMask;
        Heap::RootListIndex root_index = static_cast<Heap::RootListIndex>(id);
        MaybeObject* object =
            MaybeObject::FromObject(isolate->heap()->root(root_index));
        DCHECK(!isolate->heap()->InNewSpace(object));
        DCHECK(!allocator()->next_reference_is_weak());
        UnalignedCopy(current++, &object);
        break;
      }

      STATIC_ASSERT(kNumberOfHotObjects == 8);
      FOUR_CASES(kHotObjectWithSkip)
      FOUR_CASES(kHotObjectWithSkip + 4) {
        int skip = source_.GetInt();
        current = reinterpret_cast<MaybeObject**>(
            reinterpret_cast<Address>(current) + skip);
        V8_FALLTHROUGH;
      }

      FOUR_CASES(kHotObject)
      FOUR_CASES(kHotObject + 4) {
        int index = data & kHotObjectMask;
        Object* hot_object = hot_objects_.Get(index);
        MaybeObject* hot_maybe_object = MaybeObject::FromObject(hot_object);
        if (allocator()->GetAndClearNextReferenceIsWeak()) {
          hot_maybe_object = MaybeObject::MakeWeak(hot_maybe_object);
        }

        UnalignedCopy(current, &hot_maybe_object);
        if (write_barrier_needed && isolate->heap()->InNewSpace(hot_object)) {
          Address current_address = reinterpret_cast<Address>(current);
          isolate->heap()->RecordWrite(
              HeapObject::FromAddress(current_object_address),
              reinterpret_cast<MaybeObject**>(current_address),
              hot_maybe_object);
        }
        current++;
        break;
      }

      // Deserialize raw data of fixed length from 1 to 32 words.
      STATIC_ASSERT(kNumberOfFixedRawData == 32);
      SIXTEEN_CASES(kFixedRawData)
      SIXTEEN_CASES(kFixedRawData + 16) {
        byte* raw_data_out = reinterpret_cast<byte*>(current);
        int size_in_bytes = (data - kFixedRawDataStart) << kPointerSizeLog2;
        source_.CopyRaw(raw_data_out, size_in_bytes);
        current = reinterpret_cast<MaybeObject**>(raw_data_out + size_in_bytes);
        break;
      }

      STATIC_ASSERT(kNumberOfFixedRepeat == 16);
      SIXTEEN_CASES(kFixedRepeat) {
        int repeats = data - kFixedRepeatStart;
        MaybeObject* object;
        DCHECK(!allocator()->next_reference_is_weak());
        UnalignedCopy(&object, current - 1);
        DCHECK(!isolate->heap()->InNewSpace(object));
        for (int i = 0; i < repeats; i++) UnalignedCopy(current++, &object);
        break;
      }

#ifdef DEBUG
#define UNUSED_CASE(byte_code) \
  case byte_code:              \
    UNREACHABLE();
      UNUSED_SERIALIZER_BYTE_CODES(UNUSED_CASE)
#endif
#undef UNUSED_CASE

#undef SIXTEEN_CASES
#undef FOUR_CASES
#undef SINGLE_CASE
    }
  }
  CHECK_EQ(limit, current);
  return true;
}

namespace {

int FixupJSConstructStub(Isolate* isolate, int builtin_id) {
  if (isolate->serializer_enabled()) return builtin_id;

  if (FLAG_harmony_restrict_constructor_return &&
      builtin_id == Builtins::kJSConstructStubGenericUnrestrictedReturn) {
    return Builtins::kJSConstructStubGenericRestrictedReturn;
  } else if (!FLAG_harmony_restrict_constructor_return &&
             builtin_id == Builtins::kJSConstructStubGenericRestrictedReturn) {
    return Builtins::kJSConstructStubGenericUnrestrictedReturn;
  } else {
    return builtin_id;
  }
}

}  // namespace

template <class AllocatorT>
void** Deserializer<AllocatorT>::ReadExternalReferenceCase(
    HowToCode how, Isolate* isolate, void** current,
    Address current_object_address) {
  int skip = source_.GetInt();
  current = reinterpret_cast<void**>(reinterpret_cast<Address>(current) + skip);
  uint32_t reference_id = static_cast<uint32_t>(source_.GetInt());
  Address address = external_reference_table_->address(reference_id);

  if (how == kFromCode) {
    Address location_of_branch_data = reinterpret_cast<Address>(current);
    Assembler::deserialization_set_special_target_at(
        location_of_branch_data,
        Code::cast(HeapObject::FromAddress(current_object_address)), address);
    location_of_branch_data += Assembler::kSpecialTargetSize;
    current = reinterpret_cast<void**>(location_of_branch_data);
  } else {
    void* new_current = reinterpret_cast<void**>(address);
    UnalignedCopy(current, &new_current);
    ++current;
  }
  return current;
}

template <class AllocatorT>
template <int where, int how, int within, int space_number_if_any>
MaybeObject** Deserializer<AllocatorT>::ReadDataCase(
    Isolate* isolate, MaybeObject** current, Address current_object_address,
    byte data, bool write_barrier_needed) {
  bool emit_write_barrier = false;
  bool current_was_incremented = false;
  int space_number = space_number_if_any == kAnyOldSpace ? (data & kSpaceMask)
                                                         : space_number_if_any;
  HeapObjectReferenceType reference_type = HeapObjectReferenceType::STRONG;
  if (where == kNewObject && how == kPlain && within == kStartOfObject) {
    if (allocator()->GetAndClearNextReferenceIsWeak()) {
      reference_type = HeapObjectReferenceType::WEAK;
    }
    ReadObject(space_number, current, reference_type);
    emit_write_barrier = (space_number == NEW_SPACE);
  } else {
    Object* new_object = nullptr; /* May not be a real Object pointer. */
    if (where == kNewObject) {
      ReadObject(space_number, reinterpret_cast<MaybeObject**>(&new_object),
                 HeapObjectReferenceType::STRONG);
    } else if (where == kBackref) {
      emit_write_barrier = (space_number == NEW_SPACE);
      new_object = GetBackReferencedObject(data & kSpaceMask);
    } else if (where == kBackrefWithSkip) {
      int skip = source_.GetInt();
      current = reinterpret_cast<MaybeObject**>(
          reinterpret_cast<Address>(current) + skip);
      emit_write_barrier = (space_number == NEW_SPACE);
      new_object = GetBackReferencedObject(data & kSpaceMask);
    } else if (where == kRootArray) {
      int id = source_.GetInt();
      Heap::RootListIndex root_index = static_cast<Heap::RootListIndex>(id);
      new_object = isolate->heap()->root(root_index);
      emit_write_barrier = isolate->heap()->InNewSpace(new_object);
      hot_objects_.Add(HeapObject::cast(new_object));
    } else if (where == kPartialSnapshotCache) {
      int cache_index = source_.GetInt();
      new_object = isolate->partial_snapshot_cache()->at(cache_index);
      emit_write_barrier = isolate->heap()->InNewSpace(new_object);
    } else if (where == kAttachedReference) {
      int index = source_.GetInt();
      new_object = *attached_objects_[index];
      emit_write_barrier = isolate->heap()->InNewSpace(new_object);
    } else {
      DCHECK_EQ(where, kBuiltin);
      int raw_id = MaybeReplaceWithDeserializeLazy(source_.GetInt());
      int builtin_id = FixupJSConstructStub(isolate, raw_id);
      new_object = isolate->builtins()->builtin(builtin_id);
      emit_write_barrier = false;
    }
    if (within == kInnerPointer) {
      DCHECK_EQ(how, kFromCode);
      if (where == kBuiltin) {
        // At this point, new_object may still be uninitialized, thus the
        // unchecked Code cast.
        new_object = reinterpret_cast<Object*>(
            reinterpret_cast<Code*>(new_object)->raw_instruction_start());
      } else if (new_object->IsCode()) {
        new_object = reinterpret_cast<Object*>(
            Code::cast(new_object)->raw_instruction_start());
      } else {
        Cell* cell = Cell::cast(new_object);
        new_object = reinterpret_cast<Object*>(cell->ValueAddress());
      }
    }
    if (how == kFromCode) {
      DCHECK(!allocator()->next_reference_is_weak());
      Address location_of_branch_data = reinterpret_cast<Address>(current);
      Assembler::deserialization_set_special_target_at(
          location_of_branch_data,
          Code::cast(HeapObject::FromAddress(current_object_address)),
          reinterpret_cast<Address>(new_object));
      location_of_branch_data += Assembler::kSpecialTargetSize;
      current = reinterpret_cast<MaybeObject**>(location_of_branch_data);
      current_was_incremented = true;
    } else {
      MaybeObject* new_maybe_object = MaybeObject::FromObject(new_object);
      if (allocator()->GetAndClearNextReferenceIsWeak()) {
        new_maybe_object = MaybeObject::MakeWeak(new_maybe_object);
      }
      UnalignedCopy(current, &new_maybe_object);
    }
  }
  if (emit_write_barrier && write_barrier_needed) {
    Address current_address = reinterpret_cast<Address>(current);
    SLOW_DCHECK(isolate->heap()->ContainsSlow(current_object_address));
    isolate->heap()->RecordWrite(
        HeapObject::FromAddress(current_object_address),
        reinterpret_cast<MaybeObject**>(current_address),
        *reinterpret_cast<MaybeObject**>(current_address));
  }
  if (!current_was_incremented) {
    current++;
  }

  return current;
}

// Explicit instantiation.
template class Deserializer<BuiltinDeserializerAllocator>;
template class Deserializer<DefaultDeserializerAllocator>;

}  // namespace internal
}  // namespace v8
