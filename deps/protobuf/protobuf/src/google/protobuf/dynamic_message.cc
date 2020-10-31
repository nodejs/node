// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// DynamicMessage is implemented by constructing a data structure which
// has roughly the same memory layout as a generated message would have.
// Then, we use Reflection to implement our reflection interface.  All
// the other operations we need to implement (e.g.  parsing, copying,
// etc.) are already implemented in terms of Reflection, so the rest is
// easy.
//
// The up side of this strategy is that it's very efficient.  We don't
// need to use hash_maps or generic representations of fields.  The
// down side is that this is a low-level memory management hack which
// can be tricky to get right.
//
// As mentioned in the header, we only expose a DynamicMessageFactory
// publicly, not the DynamicMessage class itself.  This is because
// GenericMessageReflection wants to have a pointer to a "default"
// copy of the class, with all fields initialized to their default
// values.  We only want to construct one of these per message type,
// so DynamicMessageFactory stores a cache of default messages for
// each type it sees (each unique Descriptor pointer).  The code
// refers to the "default" copy of the class as the "prototype".
//
// Note on memory allocation:  This module often calls "operator new()"
// to allocate untyped memory, rather than calling something like
// "new uint8[]".  This is because "operator new()" means "Give me some
// space which I can use as I please." while "new uint8[]" means "Give
// me an array of 8-bit integers.".  In practice, the later may return
// a pointer that is not aligned correctly for general use.  I believe
// Item 8 of "More Effective C++" discusses this in more detail, though
// I don't have the book on me right now so I'm not sure.

#include <algorithm>
#include <memory>
#include <unordered_map>

#include <google/protobuf/stubs/hash.h>

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/map_field.h>
#include <google/protobuf/map_field_inl.h>
#include <google/protobuf/map_type_handler.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/wire_format.h>

namespace google {
namespace protobuf {

using internal::DynamicMapField;
using internal::ExtensionSet;
using internal::InternalMetadataWithArena;
using internal::MapField;


using internal::ArenaStringPtr;

// ===================================================================
// Some helper tables and functions...

namespace {

bool IsMapFieldInApi(const FieldDescriptor* field) { return field->is_map(); }

// Compute the byte size of the in-memory representation of the field.
int FieldSpaceUsed(const FieldDescriptor* field) {
  typedef FieldDescriptor FD;  // avoid line wrapping
  if (field->label() == FD::LABEL_REPEATED) {
    switch (field->cpp_type()) {
      case FD::CPPTYPE_INT32:
        return sizeof(RepeatedField<int32>);
      case FD::CPPTYPE_INT64:
        return sizeof(RepeatedField<int64>);
      case FD::CPPTYPE_UINT32:
        return sizeof(RepeatedField<uint32>);
      case FD::CPPTYPE_UINT64:
        return sizeof(RepeatedField<uint64>);
      case FD::CPPTYPE_DOUBLE:
        return sizeof(RepeatedField<double>);
      case FD::CPPTYPE_FLOAT:
        return sizeof(RepeatedField<float>);
      case FD::CPPTYPE_BOOL:
        return sizeof(RepeatedField<bool>);
      case FD::CPPTYPE_ENUM:
        return sizeof(RepeatedField<int>);
      case FD::CPPTYPE_MESSAGE:
        if (IsMapFieldInApi(field)) {
          return sizeof(DynamicMapField);
        } else {
          return sizeof(RepeatedPtrField<Message>);
        }

      case FD::CPPTYPE_STRING:
        switch (field->options().ctype()) {
          default:  // TODO(kenton):  Support other string reps.
          case FieldOptions::STRING:
            return sizeof(RepeatedPtrField<std::string>);
        }
        break;
    }
  } else {
    switch (field->cpp_type()) {
      case FD::CPPTYPE_INT32:
        return sizeof(int32);
      case FD::CPPTYPE_INT64:
        return sizeof(int64);
      case FD::CPPTYPE_UINT32:
        return sizeof(uint32);
      case FD::CPPTYPE_UINT64:
        return sizeof(uint64);
      case FD::CPPTYPE_DOUBLE:
        return sizeof(double);
      case FD::CPPTYPE_FLOAT:
        return sizeof(float);
      case FD::CPPTYPE_BOOL:
        return sizeof(bool);
      case FD::CPPTYPE_ENUM:
        return sizeof(int);

      case FD::CPPTYPE_MESSAGE:
        return sizeof(Message*);

      case FD::CPPTYPE_STRING:
        switch (field->options().ctype()) {
          default:  // TODO(kenton):  Support other string reps.
          case FieldOptions::STRING:
            return sizeof(ArenaStringPtr);
        }
        break;
    }
  }

  GOOGLE_LOG(DFATAL) << "Can't get here.";
  return 0;
}

// Compute the byte size of in-memory representation of the oneof fields
// in default oneof instance.
int OneofFieldSpaceUsed(const FieldDescriptor* field) {
  typedef FieldDescriptor FD;  // avoid line wrapping
  switch (field->cpp_type()) {
    case FD::CPPTYPE_INT32:
      return sizeof(int32);
    case FD::CPPTYPE_INT64:
      return sizeof(int64);
    case FD::CPPTYPE_UINT32:
      return sizeof(uint32);
    case FD::CPPTYPE_UINT64:
      return sizeof(uint64);
    case FD::CPPTYPE_DOUBLE:
      return sizeof(double);
    case FD::CPPTYPE_FLOAT:
      return sizeof(float);
    case FD::CPPTYPE_BOOL:
      return sizeof(bool);
    case FD::CPPTYPE_ENUM:
      return sizeof(int);

    case FD::CPPTYPE_MESSAGE:
      return sizeof(Message*);

    case FD::CPPTYPE_STRING:
      switch (field->options().ctype()) {
        default:
        case FieldOptions::STRING:
          return sizeof(ArenaStringPtr);
      }
      break;
  }

  GOOGLE_LOG(DFATAL) << "Can't get here.";
  return 0;
}

inline int DivideRoundingUp(int i, int j) { return (i + (j - 1)) / j; }

static const int kSafeAlignment = sizeof(uint64);
static const int kMaxOneofUnionSize = sizeof(uint64);

inline int AlignTo(int offset, int alignment) {
  return DivideRoundingUp(offset, alignment) * alignment;
}

// Rounds the given byte offset up to the next offset aligned such that any
// type may be stored at it.
inline int AlignOffset(int offset) { return AlignTo(offset, kSafeAlignment); }

#define bitsizeof(T) (sizeof(T) * 8)

}  // namespace

// ===================================================================

class DynamicMessage : public Message {
 public:
  struct TypeInfo {
    int size;
    int has_bits_offset;
    int oneof_case_offset;
    int internal_metadata_offset;
    int extensions_offset;

    // Not owned by the TypeInfo.
    DynamicMessageFactory* factory;  // The factory that created this object.
    const DescriptorPool* pool;      // The factory's DescriptorPool.
    const Descriptor* type;          // Type of this DynamicMessage.

    // Warning:  The order in which the following pointers are defined is
    //   important (the prototype must be deleted *before* the offsets).
    std::unique_ptr<uint32[]> offsets;
    std::unique_ptr<uint32[]> has_bits_indices;
    std::unique_ptr<const Reflection> reflection;
    // Don't use a unique_ptr to hold the prototype: the destructor for
    // DynamicMessage needs to know whether it is the prototype, and does so by
    // looking back at this field. This would assume details about the
    // implementation of unique_ptr.
    const DynamicMessage* prototype;
    int weak_field_map_offset;  // The offset for the weak_field_map;

    TypeInfo() : prototype(NULL) {}

    ~TypeInfo() { delete prototype; }
  };

  DynamicMessage(const TypeInfo* type_info);

  // This should only be used by GetPrototypeNoLock() to avoid dead lock.
  DynamicMessage(TypeInfo* type_info, bool lock_factory);

  ~DynamicMessage();

  // Called on the prototype after construction to initialize message fields.
  void CrossLinkPrototypes();

  // implements Message ----------------------------------------------

  Message* New() const override;
  Message* New(Arena* arena) const override;
  Arena* GetArena() const override { return arena_; }

  int GetCachedSize() const override;
  void SetCachedSize(int size) const override;

  Metadata GetMetadata() const override;

  // We actually allocate more memory than sizeof(*this) when this
  // class's memory is allocated via the global operator new. Thus, we need to
  // manually call the global operator delete. Calling the destructor is taken
  // care of for us. This makes DynamicMessage compatible with -fsized-delete.
  // It doesn't work for MSVC though.
#ifndef _MSC_VER
  static void operator delete(void* ptr) { ::operator delete(ptr); }
#endif  // !_MSC_VER

 private:
  DynamicMessage(const TypeInfo* type_info, Arena* arena);

  void SharedCtor(bool lock_factory);

  inline bool is_prototype() const {
    return type_info_->prototype == this ||
           // If type_info_->prototype is NULL, then we must be constructing
           // the prototype now, which means we must be the prototype.
           type_info_->prototype == NULL;
  }

  inline void* OffsetToPointer(int offset) {
    return reinterpret_cast<uint8*>(this) + offset;
  }
  inline const void* OffsetToPointer(int offset) const {
    return reinterpret_cast<const uint8*>(this) + offset;
  }

  const TypeInfo* type_info_;
  Arena* const arena_;
  mutable std::atomic<int> cached_byte_size_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(DynamicMessage);
};

DynamicMessage::DynamicMessage(const TypeInfo* type_info)
    : type_info_(type_info), arena_(NULL), cached_byte_size_(0) {
  SharedCtor(true);
}

DynamicMessage::DynamicMessage(const TypeInfo* type_info, Arena* arena)
    : type_info_(type_info), arena_(arena), cached_byte_size_(0) {
  SharedCtor(true);
}

DynamicMessage::DynamicMessage(TypeInfo* type_info, bool lock_factory)
    : type_info_(type_info), arena_(NULL), cached_byte_size_(0) {
  // The prototype in type_info has to be set before creating the prototype
  // instance on memory. e.g., message Foo { map<int32, Foo> a = 1; }. When
  // creating prototype for Foo, prototype of the map entry will also be
  // created, which needs the address of the prototype of Foo (the value in
  // map). To break the cyclic dependency, we have to assign the address of
  // prototype into type_info first.
  type_info->prototype = this;
  SharedCtor(lock_factory);
}

void DynamicMessage::SharedCtor(bool lock_factory) {
  // We need to call constructors for various fields manually and set
  // default values where appropriate.  We use placement new to call
  // constructors.  If you haven't heard of placement new, I suggest Googling
  // it now.  We use placement new even for primitive types that don't have
  // constructors for consistency.  (In theory, placement new should be used
  // any time you are trying to convert untyped memory to typed memory, though
  // in practice that's not strictly necessary for types that don't have a
  // constructor.)

  const Descriptor* descriptor = type_info_->type;
  // Initialize oneof cases.
  for (int i = 0; i < descriptor->oneof_decl_count(); ++i) {
    new (OffsetToPointer(type_info_->oneof_case_offset + sizeof(uint32) * i))
        uint32(0);
  }

  new (OffsetToPointer(type_info_->internal_metadata_offset))
      InternalMetadataWithArena(arena_);

  if (type_info_->extensions_offset != -1) {
    new (OffsetToPointer(type_info_->extensions_offset)) ExtensionSet(arena_);
  }
  for (int i = 0; i < descriptor->field_count(); i++) {
    const FieldDescriptor* field = descriptor->field(i);
    void* field_ptr = OffsetToPointer(type_info_->offsets[i]);
    if (field->containing_oneof()) {
      continue;
    }
    switch (field->cpp_type()) {
#define HANDLE_TYPE(CPPTYPE, TYPE)                         \
  case FieldDescriptor::CPPTYPE_##CPPTYPE:                 \
    if (!field->is_repeated()) {                           \
      new (field_ptr) TYPE(field->default_value_##TYPE()); \
    } else {                                               \
      new (field_ptr) RepeatedField<TYPE>(arena_);         \
    }                                                      \
    break;

      HANDLE_TYPE(INT32, int32);
      HANDLE_TYPE(INT64, int64);
      HANDLE_TYPE(UINT32, uint32);
      HANDLE_TYPE(UINT64, uint64);
      HANDLE_TYPE(DOUBLE, double);
      HANDLE_TYPE(FLOAT, float);
      HANDLE_TYPE(BOOL, bool);
#undef HANDLE_TYPE

      case FieldDescriptor::CPPTYPE_ENUM:
        if (!field->is_repeated()) {
          new (field_ptr) int(field->default_value_enum()->number());
        } else {
          new (field_ptr) RepeatedField<int>(arena_);
        }
        break;

      case FieldDescriptor::CPPTYPE_STRING:
        switch (field->options().ctype()) {
          default:  // TODO(kenton):  Support other string reps.
          case FieldOptions::STRING:
            if (!field->is_repeated()) {
              const std::string* default_value;
              if (is_prototype()) {
                default_value = &field->default_value_string();
              } else {
                default_value = &(reinterpret_cast<const ArenaStringPtr*>(
                                      type_info_->prototype->OffsetToPointer(
                                          type_info_->offsets[i]))
                                      ->Get());
              }
              ArenaStringPtr* asp = new (field_ptr) ArenaStringPtr();
              asp->UnsafeSetDefault(default_value);
            } else {
              new (field_ptr) RepeatedPtrField<std::string>(arena_);
            }
            break;
        }
        break;

      case FieldDescriptor::CPPTYPE_MESSAGE: {
        if (!field->is_repeated()) {
          new (field_ptr) Message*(NULL);
        } else {
          if (IsMapFieldInApi(field)) {
            // We need to lock in most cases to avoid data racing. Only not lock
            // when the constructor is called inside GetPrototype(), in which
            // case we have already locked the factory.
            if (lock_factory) {
              if (arena_ != NULL) {
                new (field_ptr) DynamicMapField(
                    type_info_->factory->GetPrototype(field->message_type()),
                    arena_);
              } else {
                new (field_ptr) DynamicMapField(
                    type_info_->factory->GetPrototype(field->message_type()));
              }
            } else {
              if (arena_ != NULL) {
                new (field_ptr)
                    DynamicMapField(type_info_->factory->GetPrototypeNoLock(
                                        field->message_type()),
                                    arena_);
              } else {
                new (field_ptr)
                    DynamicMapField(type_info_->factory->GetPrototypeNoLock(
                        field->message_type()));
              }
            }
          } else {
            new (field_ptr) RepeatedPtrField<Message>(arena_);
          }
        }
        break;
      }
    }
  }
}

DynamicMessage::~DynamicMessage() {
  const Descriptor* descriptor = type_info_->type;

  reinterpret_cast<InternalMetadataWithArena*>(
      OffsetToPointer(type_info_->internal_metadata_offset))
      ->~InternalMetadataWithArena();

  if (type_info_->extensions_offset != -1) {
    reinterpret_cast<ExtensionSet*>(
        OffsetToPointer(type_info_->extensions_offset))
        ->~ExtensionSet();
  }

  // We need to manually run the destructors for repeated fields and strings,
  // just as we ran their constructors in the DynamicMessage constructor.
  // We also need to manually delete oneof fields if it is set and is string
  // or message.
  // Additionally, if any singular embedded messages have been allocated, we
  // need to delete them, UNLESS we are the prototype message of this type,
  // in which case any embedded messages are other prototypes and shouldn't
  // be touched.
  for (int i = 0; i < descriptor->field_count(); i++) {
    const FieldDescriptor* field = descriptor->field(i);
    if (field->containing_oneof()) {
      void* field_ptr =
          OffsetToPointer(type_info_->oneof_case_offset +
                          sizeof(uint32) * field->containing_oneof()->index());
      if (*(reinterpret_cast<const uint32*>(field_ptr)) == field->number()) {
        field_ptr = OffsetToPointer(
            type_info_->offsets[descriptor->field_count() +
                                field->containing_oneof()->index()]);
        if (field->cpp_type() == FieldDescriptor::CPPTYPE_STRING) {
          switch (field->options().ctype()) {
            default:
            case FieldOptions::STRING: {
              const std::string* default_value =
                  &(reinterpret_cast<const ArenaStringPtr*>(
                        reinterpret_cast<const uint8*>(type_info_->prototype) +
                        type_info_->offsets[i])
                        ->Get());
              reinterpret_cast<ArenaStringPtr*>(field_ptr)->Destroy(
                  default_value, NULL);
              break;
            }
          }
        } else if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
            delete *reinterpret_cast<Message**>(field_ptr);
        }
      }
      continue;
    }
    void* field_ptr = OffsetToPointer(type_info_->offsets[i]);

    if (field->is_repeated()) {
      switch (field->cpp_type()) {
#define HANDLE_TYPE(UPPERCASE, LOWERCASE)                  \
  case FieldDescriptor::CPPTYPE_##UPPERCASE:               \
    reinterpret_cast<RepeatedField<LOWERCASE>*>(field_ptr) \
        ->~RepeatedField<LOWERCASE>();                     \
    break

        HANDLE_TYPE(INT32, int32);
        HANDLE_TYPE(INT64, int64);
        HANDLE_TYPE(UINT32, uint32);
        HANDLE_TYPE(UINT64, uint64);
        HANDLE_TYPE(DOUBLE, double);
        HANDLE_TYPE(FLOAT, float);
        HANDLE_TYPE(BOOL, bool);
        HANDLE_TYPE(ENUM, int);
#undef HANDLE_TYPE

        case FieldDescriptor::CPPTYPE_STRING:
          switch (field->options().ctype()) {
            default:  // TODO(kenton):  Support other string reps.
            case FieldOptions::STRING:
              reinterpret_cast<RepeatedPtrField<std::string>*>(field_ptr)
                  ->~RepeatedPtrField<std::string>();
              break;
          }
          break;

        case FieldDescriptor::CPPTYPE_MESSAGE:
          if (IsMapFieldInApi(field)) {
            reinterpret_cast<DynamicMapField*>(field_ptr)->~DynamicMapField();
          } else {
            reinterpret_cast<RepeatedPtrField<Message>*>(field_ptr)
                ->~RepeatedPtrField<Message>();
          }
          break;
      }

    } else if (field->cpp_type() == FieldDescriptor::CPPTYPE_STRING) {
      switch (field->options().ctype()) {
        default:  // TODO(kenton):  Support other string reps.
        case FieldOptions::STRING: {
          const std::string* default_value =
              &(reinterpret_cast<const ArenaStringPtr*>(
                    type_info_->prototype->OffsetToPointer(
                        type_info_->offsets[i]))
                    ->Get());
          reinterpret_cast<ArenaStringPtr*>(field_ptr)->Destroy(default_value,
                                                                NULL);
          break;
        }
      }
    } else if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
          if (!is_prototype()) {
        Message* message = *reinterpret_cast<Message**>(field_ptr);
        if (message != NULL) {
          delete message;
        }
      }
    }
  }
}

void DynamicMessage::CrossLinkPrototypes() {
  // This should only be called on the prototype message.
  GOOGLE_CHECK(is_prototype());

  DynamicMessageFactory* factory = type_info_->factory;
  const Descriptor* descriptor = type_info_->type;

  // Cross-link default messages.
  for (int i = 0; i < descriptor->field_count(); i++) {
    const FieldDescriptor* field = descriptor->field(i);
    void* field_ptr = OffsetToPointer(type_info_->offsets[i]);
    if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE &&
        !field->is_repeated()) {
      // For fields with message types, we need to cross-link with the
      // prototype for the field's type.
      // For singular fields, the field is just a pointer which should
      // point to the prototype.
      *reinterpret_cast<const Message**>(field_ptr) =
          factory->GetPrototypeNoLock(field->message_type());
    }
  }
}

Message* DynamicMessage::New() const { return New(NULL); }

Message* DynamicMessage::New(Arena* arena) const {
  if (arena != NULL) {
    void* new_base = Arena::CreateArray<char>(arena, type_info_->size);
    memset(new_base, 0, type_info_->size);
    return new (new_base) DynamicMessage(type_info_, arena);
  } else {
    void* new_base = operator new(type_info_->size);
    memset(new_base, 0, type_info_->size);
    return new (new_base) DynamicMessage(type_info_);
  }
}

int DynamicMessage::GetCachedSize() const {
  return cached_byte_size_.load(std::memory_order_relaxed);
}

void DynamicMessage::SetCachedSize(int size) const {
  cached_byte_size_.store(size, std::memory_order_relaxed);
}

Metadata DynamicMessage::GetMetadata() const {
  Metadata metadata;
  metadata.descriptor = type_info_->type;
  metadata.reflection = type_info_->reflection.get();
  return metadata;
}

// ===================================================================

struct DynamicMessageFactory::PrototypeMap {
  typedef std::unordered_map<const Descriptor*, const DynamicMessage::TypeInfo*>
      Map;
  Map map_;
};

DynamicMessageFactory::DynamicMessageFactory()
    : pool_(NULL),
      delegate_to_generated_factory_(false),
      prototypes_(new PrototypeMap) {}

DynamicMessageFactory::DynamicMessageFactory(const DescriptorPool* pool)
    : pool_(pool),
      delegate_to_generated_factory_(false),
      prototypes_(new PrototypeMap) {}

DynamicMessageFactory::~DynamicMessageFactory() {
  for (PrototypeMap::Map::iterator iter = prototypes_->map_.begin();
       iter != prototypes_->map_.end(); ++iter) {
    DeleteDefaultOneofInstance(iter->second->type, iter->second->offsets.get(),
                               iter->second->prototype);
    delete iter->second;
  }
}

const Message* DynamicMessageFactory::GetPrototype(const Descriptor* type) {
  MutexLock lock(&prototypes_mutex_);
  return GetPrototypeNoLock(type);
}

const Message* DynamicMessageFactory::GetPrototypeNoLock(
    const Descriptor* type) {
  if (delegate_to_generated_factory_ &&
      type->file()->pool() == DescriptorPool::generated_pool()) {
    return MessageFactory::generated_factory()->GetPrototype(type);
  }

  const DynamicMessage::TypeInfo** target = &prototypes_->map_[type];
  if (*target != NULL) {
    // Already exists.
    return (*target)->prototype;
  }

  DynamicMessage::TypeInfo* type_info = new DynamicMessage::TypeInfo;
  *target = type_info;

  type_info->type = type;
  type_info->pool = (pool_ == NULL) ? type->file()->pool() : pool_;
  type_info->factory = this;

  // We need to construct all the structures passed to Reflection's constructor.
  // This includes:
  // - A block of memory that contains space for all the message's fields.
  // - An array of integers indicating the byte offset of each field within
  //   this block.
  // - A big bitfield containing a bit for each field indicating whether
  //   or not that field is set.

  // Compute size and offsets.
  uint32* offsets = new uint32[type->field_count() + type->oneof_decl_count()];
  type_info->offsets.reset(offsets);

  // Decide all field offsets by packing in order.
  // We place the DynamicMessage object itself at the beginning of the allocated
  // space.
  int size = sizeof(DynamicMessage);
  size = AlignOffset(size);

  // Next the has_bits, which is an array of uint32s.
  if (type->file()->syntax() == FileDescriptor::SYNTAX_PROTO3) {
    type_info->has_bits_offset = -1;
  } else {
    type_info->has_bits_offset = size;
    int has_bits_array_size =
        DivideRoundingUp(type->field_count(), bitsizeof(uint32));
    size += has_bits_array_size * sizeof(uint32);
    size = AlignOffset(size);

    uint32* has_bits_indices = new uint32[type->field_count()];
    for (int i = 0; i < type->field_count(); i++) {
      has_bits_indices[i] = i;
    }
    type_info->has_bits_indices.reset(has_bits_indices);
  }

  // The oneof_case, if any. It is an array of uint32s.
  if (type->oneof_decl_count() > 0) {
    type_info->oneof_case_offset = size;
    size += type->oneof_decl_count() * sizeof(uint32);
    size = AlignOffset(size);
  }

  // The ExtensionSet, if any.
  if (type->extension_range_count() > 0) {
    type_info->extensions_offset = size;
    size += sizeof(ExtensionSet);
    size = AlignOffset(size);
  } else {
    // No extensions.
    type_info->extensions_offset = -1;
  }

  // All the fields.
  //
  // TODO(b/31226269):  Optimize the order of fields to minimize padding.
  int num_weak_fields = 0;
  for (int i = 0; i < type->field_count(); i++) {
    // Make sure field is aligned to avoid bus errors.
    // Oneof fields do not use any space.
    if (!type->field(i)->containing_oneof()) {
      int field_size = FieldSpaceUsed(type->field(i));
      size = AlignTo(size, std::min(kSafeAlignment, field_size));
      offsets[i] = size;
      size += field_size;
    }
  }

  // The oneofs.
  for (int i = 0; i < type->oneof_decl_count(); i++) {
    size = AlignTo(size, kSafeAlignment);
    offsets[type->field_count() + i] = size;
    size += kMaxOneofUnionSize;
  }

  // Add the InternalMetadataWithArena to the end.
  size = AlignOffset(size);
  type_info->internal_metadata_offset = size;
  size += sizeof(InternalMetadataWithArena);

  type_info->weak_field_map_offset = -1;

  // Align the final size to make sure no clever allocators think that
  // alignment is not necessary.
  type_info->size = size;

  // Construct the reflection object.

  if (type->oneof_decl_count() > 0) {
    // Compute the size of default oneof instance and offsets of default
    // oneof fields.
    for (int i = 0; i < type->oneof_decl_count(); i++) {
      for (int j = 0; j < type->oneof_decl(i)->field_count(); j++) {
        const FieldDescriptor* field = type->oneof_decl(i)->field(j);
        int field_size = OneofFieldSpaceUsed(field);
        size = AlignTo(size, std::min(kSafeAlignment, field_size));
        offsets[field->index()] = size;
        size += field_size;
      }
    }
  }
  size = AlignOffset(size);
  // Allocate the prototype + oneof fields.
  void* base = operator new(size);
  memset(base, 0, size);

  // We have already locked the factory so we should not lock in the constructor
  // of dynamic message to avoid dead lock.
  DynamicMessage* prototype = new (base) DynamicMessage(type_info, false);

  if (type->oneof_decl_count() > 0 || num_weak_fields > 0) {
    // Construct default oneof instance.
    ConstructDefaultOneofInstance(type_info->type, type_info->offsets.get(),
                                  prototype);
  }

  internal::ReflectionSchema schema = {type_info->prototype,
                                       type_info->offsets.get(),
                                       type_info->has_bits_indices.get(),
                                       type_info->has_bits_offset,
                                       type_info->internal_metadata_offset,
                                       type_info->extensions_offset,
                                       type_info->oneof_case_offset,
                                       type_info->size,
                                       type_info->weak_field_map_offset};

  type_info->reflection.reset(
      new Reflection(type_info->type, schema, type_info->pool, this));

  // Cross link prototypes.
  prototype->CrossLinkPrototypes();

  return prototype;
}

void DynamicMessageFactory::ConstructDefaultOneofInstance(
    const Descriptor* type, const uint32 offsets[],
    void* default_oneof_or_weak_instance) {
  for (int i = 0; i < type->oneof_decl_count(); i++) {
    for (int j = 0; j < type->oneof_decl(i)->field_count(); j++) {
      const FieldDescriptor* field = type->oneof_decl(i)->field(j);
      void* field_ptr =
          reinterpret_cast<uint8*>(default_oneof_or_weak_instance) +
          offsets[field->index()];
      switch (field->cpp_type()) {
#define HANDLE_TYPE(CPPTYPE, TYPE)                       \
  case FieldDescriptor::CPPTYPE_##CPPTYPE:               \
    new (field_ptr) TYPE(field->default_value_##TYPE()); \
    break;

        HANDLE_TYPE(INT32, int32);
        HANDLE_TYPE(INT64, int64);
        HANDLE_TYPE(UINT32, uint32);
        HANDLE_TYPE(UINT64, uint64);
        HANDLE_TYPE(DOUBLE, double);
        HANDLE_TYPE(FLOAT, float);
        HANDLE_TYPE(BOOL, bool);
#undef HANDLE_TYPE

        case FieldDescriptor::CPPTYPE_ENUM:
          new (field_ptr) int(field->default_value_enum()->number());
          break;
        case FieldDescriptor::CPPTYPE_STRING:
          switch (field->options().ctype()) {
            default:
            case FieldOptions::STRING:
              ArenaStringPtr* asp = new (field_ptr) ArenaStringPtr();
              asp->UnsafeSetDefault(&field->default_value_string());
              break;
          }
          break;

        case FieldDescriptor::CPPTYPE_MESSAGE: {
          new (field_ptr) Message*(NULL);
          break;
        }
      }
    }
  }
}

void DynamicMessageFactory::DeleteDefaultOneofInstance(
    const Descriptor* type, const uint32 offsets[],
    const void* default_oneof_instance) {
  for (int i = 0; i < type->oneof_decl_count(); i++) {
    for (int j = 0; j < type->oneof_decl(i)->field_count(); j++) {
      const FieldDescriptor* field = type->oneof_decl(i)->field(j);
      if (field->cpp_type() == FieldDescriptor::CPPTYPE_STRING) {
        switch (field->options().ctype()) {
          default:
          case FieldOptions::STRING:
            break;
        }
      }
    }
  }
}

}  // namespace protobuf
}  // namespace google
