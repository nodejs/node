// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note 1: Any file that includes this one should include object-macros-undef.h
// at the bottom.

// Note 2: This file is deliberately missing the include guards (the undeffing
// approach wouldn't work otherwise).
//
// PRESUBMIT_INTENTIONALLY_MISSING_INCLUDE_GUARD

// The accessors with RELAXED_, ACQUIRE_, and RELEASE_ prefixes should be used
// for fields that can be written to and read from multiple threads at the same
// time. See comments in src/base/atomicops.h for the memory ordering sematics.

#include "src/base/memory.h"

// Since this changes visibility, it should always be last in a class
// definition.
#define OBJECT_CONSTRUCTORS(Type, ...)             \
 public:                                           \
  constexpr Type() : __VA_ARGS__() {}              \
                                                   \
 protected:                                        \
  template <typename TFieldType, int kFieldOffset> \
  friend class TaggedField;                        \
                                                   \
  explicit inline Type(Address ptr)

#define OBJECT_CONSTRUCTORS_IMPL(Type, Super) \
  inline Type::Type(Address ptr) : Super(ptr) { SLOW_DCHECK(Is##Type()); }
// In these cases, we don't have our own instance type to check, so check the
// supertype instead. This happens for types denoting a NativeContext-dependent
// set of maps.
#define OBJECT_CONSTRUCTORS_IMPL_CHECK_SUPER(Type, Super) \
  inline Type::Type(Address ptr) : Super(ptr) { SLOW_DCHECK(Is##Super()); }

#define NEVER_READ_ONLY_SPACE   \
  inline Heap* GetHeap() const; \
  inline Isolate* GetIsolate() const;

// TODO(leszeks): Add checks in the factory that we never allocate these
// objects in RO space.
#define NEVER_READ_ONLY_SPACE_IMPL(Type)                                   \
  Heap* Type::GetHeap() const { return GetHeapFromWritableObject(*this); } \
  Isolate* Type::GetIsolate() const {                                      \
    return GetIsolateFromWritableObject(*this);                            \
  }

#define DECL_PRIMITIVE_ACCESSORS(name, type) \
  inline type name() const;                  \
  inline void set_##name(type value);

#define DECL_SYNCHRONIZED_PRIMITIVE_ACCESSORS(name, type) \
  inline type synchronized_##name() const;                \
  inline void synchronized_set_##name(type value);

#define DECL_BOOLEAN_ACCESSORS(name) DECL_PRIMITIVE_ACCESSORS(name, bool)

#define DECL_INT_ACCESSORS(name) DECL_PRIMITIVE_ACCESSORS(name, int)

#define DECL_SYNCHRONIZED_INT_ACCESSORS(name) \
  DECL_SYNCHRONIZED_PRIMITIVE_ACCESSORS(name, int)

#define DECL_INT32_ACCESSORS(name) DECL_PRIMITIVE_ACCESSORS(name, int32_t)

#define DECL_UINT16_ACCESSORS(name) \
  inline uint16_t name() const;     \
  inline void set_##name(int value);

#define DECL_INT16_ACCESSORS(name) \
  inline int16_t name() const;     \
  inline void set_##name(int16_t value);

#define DECL_UINT8_ACCESSORS(name) \
  inline uint8_t name() const;     \
  inline void set_##name(int value);

// TODO(ishell): eventually isolate-less getters should not be used anymore.
// For full pointer-mode the C++ compiler should optimize away unused isolate
// parameter.
#define DECL_GETTER(name, type) \
  inline type name() const;     \
  inline type name(const Isolate* isolate) const;

#define DEF_GETTER(holder, name, type)                     \
  type holder::name() const {                              \
    const Isolate* isolate = GetIsolateForPtrCompr(*this); \
    return holder::name(isolate);                          \
  }                                                        \
  type holder::name(const Isolate* isolate) const

#define DECL_ACCESSORS(name, type)   \
  DECL_GETTER(name, type)            \
  inline void set_##name(type value, \
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

#define DECL_CAST(Type)                                 \
  V8_INLINE static Type cast(Object object);            \
  V8_INLINE static Type unchecked_cast(Object object) { \
    return bit_cast<Type>(object);                      \
  }

#define CAST_ACCESSOR(Type) \
  Type Type::cast(Object object) { return Type(object.ptr()); }

#define INT_ACCESSORS(holder, name, offset)                   \
  int holder::name() const { return ReadField<int>(offset); } \
  void holder::set_##name(int value) { WriteField<int>(offset, value); }

#define INT32_ACCESSORS(holder, name, offset)                         \
  int32_t holder::name() const { return ReadField<int32_t>(offset); } \
  void holder::set_##name(int32_t value) { WriteField<int32_t>(offset, value); }

#define RELAXED_INT32_ACCESSORS(holder, name, offset) \
  int32_t holder::name() const {                      \
    return RELAXED_READ_INT32_FIELD(*this, offset);   \
  }                                                   \
  void holder::set_##name(int32_t value) {            \
    RELAXED_WRITE_INT32_FIELD(*this, offset, value);  \
  }

#define UINT16_ACCESSORS(holder, name, offset)                          \
  uint16_t holder::name() const { return ReadField<uint16_t>(offset); } \
  void holder::set_##name(int value) {                                  \
    DCHECK_GE(value, 0);                                                \
    DCHECK_LE(value, static_cast<uint16_t>(-1));                        \
    WriteField<uint16_t>(offset, value);                                \
  }

#define UINT8_ACCESSORS(holder, name, offset)                         \
  uint8_t holder::name() const { return ReadField<uint8_t>(offset); } \
  void holder::set_##name(int value) {                                \
    DCHECK_GE(value, 0);                                              \
    DCHECK_LE(value, static_cast<uint8_t>(-1));                       \
    WriteField<uint8_t>(offset, value);                               \
  }

#define ACCESSORS_CHECKED2(holder, name, type, offset, get_condition, \
                           set_condition)                             \
  DEF_GETTER(holder, name, type) {                                    \
    type value = TaggedField<type, offset>::load(isolate, *this);     \
    DCHECK(get_condition);                                            \
    return value;                                                     \
  }                                                                   \
  void holder::set_##name(type value, WriteBarrierMode mode) {        \
    DCHECK(set_condition);                                            \
    TaggedField<type, offset>::store(*this, value);                   \
    CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);            \
  }

#define ACCESSORS_CHECKED(holder, name, type, offset, condition) \
  ACCESSORS_CHECKED2(holder, name, type, offset, condition, condition)

#define ACCESSORS(holder, name, type, offset) \
  ACCESSORS_CHECKED(holder, name, type, offset, true)

#define SYNCHRONIZED_ACCESSORS_CHECKED2(holder, name, type, offset,       \
                                        get_condition, set_condition)     \
  DEF_GETTER(holder, name, type) {                                        \
    type value = TaggedField<type, offset>::Acquire_Load(isolate, *this); \
    DCHECK(get_condition);                                                \
    return value;                                                         \
  }                                                                       \
  void holder::set_##name(type value, WriteBarrierMode mode) {            \
    DCHECK(set_condition);                                                \
    TaggedField<type, offset>::Release_Store(*this, value);               \
    CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);                \
  }

#define SYNCHRONIZED_ACCESSORS_CHECKED(holder, name, type, offset, condition) \
  SYNCHRONIZED_ACCESSORS_CHECKED2(holder, name, type, offset, condition,      \
                                  condition)

#define SYNCHRONIZED_ACCESSORS(holder, name, type, offset) \
  SYNCHRONIZED_ACCESSORS_CHECKED(holder, name, type, offset, true)

#define WEAK_ACCESSORS_CHECKED2(holder, name, offset, get_condition,  \
                                set_condition)                        \
  DEF_GETTER(holder, name, MaybeObject) {                             \
    MaybeObject value =                                               \
        TaggedField<MaybeObject, offset>::load(isolate, *this);       \
    DCHECK(get_condition);                                            \
    return value;                                                     \
  }                                                                   \
  void holder::set_##name(MaybeObject value, WriteBarrierMode mode) { \
    DCHECK(set_condition);                                            \
    TaggedField<MaybeObject, offset>::store(*this, value);            \
    CONDITIONAL_WEAK_WRITE_BARRIER(*this, offset, value, mode);       \
  }

#define WEAK_ACCESSORS_CHECKED(holder, name, offset, condition) \
  WEAK_ACCESSORS_CHECKED2(holder, name, offset, condition, condition)

#define WEAK_ACCESSORS(holder, name, offset) \
  WEAK_ACCESSORS_CHECKED(holder, name, offset, true)

// Getter that returns a Smi as an int and writes an int as a Smi.
#define SMI_ACCESSORS_CHECKED(holder, name, offset, condition)   \
  int holder::name() const {                                     \
    DCHECK(condition);                                           \
    Smi value = TaggedField<Smi, offset>::load(*this);           \
    return value.value();                                        \
  }                                                              \
  void holder::set_##name(int value) {                           \
    DCHECK(condition);                                           \
    TaggedField<Smi, offset>::store(*this, Smi::FromInt(value)); \
  }

#define SMI_ACCESSORS(holder, name, offset) \
  SMI_ACCESSORS_CHECKED(holder, name, offset, true)

#define SYNCHRONIZED_SMI_ACCESSORS(holder, name, offset)                 \
  int holder::synchronized_##name() const {                              \
    Smi value = TaggedField<Smi, offset>::Acquire_Load(*this);           \
    return value.value();                                                \
  }                                                                      \
  void holder::synchronized_set_##name(int value) {                      \
    TaggedField<Smi, offset>::Release_Store(*this, Smi::FromInt(value)); \
  }

#define RELAXED_SMI_ACCESSORS(holder, name, offset)                      \
  int holder::relaxed_read_##name() const {                              \
    Smi value = TaggedField<Smi, offset>::Relaxed_Load(*this);           \
    return value.value();                                                \
  }                                                                      \
  void holder::relaxed_write_##name(int value) {                         \
    TaggedField<Smi, offset>::Relaxed_Store(*this, Smi::FromInt(value)); \
  }

#define BOOL_GETTER(holder, field, name, offset) \
  bool holder::name() const { return BooleanBit::get(field(), offset); }

#define BOOL_ACCESSORS(holder, field, name, offset)                      \
  bool holder::name() const { return BooleanBit::get(field(), offset); } \
  void holder::set_##name(bool value) {                                  \
    set_##field(BooleanBit::set(field(), offset, value));                \
  }

#define BIT_FIELD_ACCESSORS(holder, field, name, BitField)      \
  typename BitField::FieldType holder::name() const {           \
    return BitField::decode(field());                           \
  }                                                             \
  void holder::set_##name(typename BitField::FieldType value) { \
    set_##field(BitField::update(field(), value));              \
  }

#define INSTANCE_TYPE_CHECKER(type, forinstancetype)    \
  V8_INLINE bool Is##type(InstanceType instance_type) { \
    return instance_type == forinstancetype;            \
  }

#define TYPE_CHECKER(type, ...)                                         \
  DEF_GETTER(HeapObject, Is##type, bool) {                              \
    return InstanceTypeChecker::Is##type(map(isolate).instance_type()); \
  }

#define RELAXED_INT16_ACCESSORS(holder, name, offset) \
  int16_t holder::name() const {                      \
    return RELAXED_READ_INT16_FIELD(*this, offset);   \
  }                                                   \
  void holder::set_##name(int16_t value) {            \
    RELAXED_WRITE_INT16_FIELD(*this, offset, value);  \
  }

#define FIELD_ADDR(p, offset) ((p).ptr() + offset - kHeapObjectTag)

#define ACQUIRE_READ_FIELD(p, offset) \
  TaggedField<Object>::Acquire_Load(p, offset)

#define RELAXED_READ_FIELD(p, offset) \
  TaggedField<Object>::Relaxed_Load(p, offset)

#define RELAXED_READ_WEAK_FIELD(p, offset) \
  TaggedField<MaybeObject>::Relaxed_Load(p, offset)

#define WRITE_FIELD(p, offset, value) \
  TaggedField<Object>::store(p, offset, value)

#define RELEASE_WRITE_FIELD(p, offset, value) \
  TaggedField<Object>::Release_Store(p, offset, value)

#define RELAXED_WRITE_FIELD(p, offset, value) \
  TaggedField<Object>::Relaxed_Store(p, offset, value)

#define RELAXED_WRITE_WEAK_FIELD(p, offset, value) \
  TaggedField<MaybeObject>::Relaxed_Store(p, offset, value)

#ifdef V8_DISABLE_WRITE_BARRIERS
#define WRITE_BARRIER(object, offset, value)
#else
#define WRITE_BARRIER(object, offset, value)                       \
  do {                                                             \
    DCHECK_NOT_NULL(GetHeapFromWritableObject(object));            \
    MarkingBarrier(object, (object).RawField(offset), value);      \
    GenerationalBarrier(object, (object).RawField(offset), value); \
  } while (false)
#endif

#ifdef V8_DISABLE_WRITE_BARRIERS
#define WEAK_WRITE_BARRIER(object, offset, value)
#else
#define WEAK_WRITE_BARRIER(object, offset, value)                           \
  do {                                                                      \
    DCHECK_NOT_NULL(GetHeapFromWritableObject(object));                     \
    MarkingBarrier(object, (object).RawMaybeWeakField(offset), value);      \
    GenerationalBarrier(object, (object).RawMaybeWeakField(offset), value); \
  } while (false)
#endif

#ifdef V8_DISABLE_WRITE_BARRIERS
#define EPHEMERON_KEY_WRITE_BARRIER(object, offset, value)
#else
#define EPHEMERON_KEY_WRITE_BARRIER(object, offset, value)                    \
  do {                                                                        \
    DCHECK_NOT_NULL(GetHeapFromWritableObject(object));                       \
    EphemeronHashTable table = EphemeronHashTable::cast(object);              \
    MarkingBarrier(object, (object).RawField(offset), value);                 \
    GenerationalEphemeronKeyBarrier(table, (object).RawField(offset), value); \
  } while (false)
#endif

#ifdef V8_DISABLE_WRITE_BARRIERS
#define CONDITIONAL_WRITE_BARRIER(object, offset, value, mode)
#else
#define CONDITIONAL_WRITE_BARRIER(object, offset, value, mode)       \
  do {                                                               \
    DCHECK_NOT_NULL(GetHeapFromWritableObject(object));              \
    DCHECK_NE(mode, UPDATE_EPHEMERON_KEY_WRITE_BARRIER);             \
    if (mode != SKIP_WRITE_BARRIER) {                                \
      if (mode == UPDATE_WRITE_BARRIER) {                            \
        MarkingBarrier(object, (object).RawField(offset), value);    \
      }                                                              \
      GenerationalBarrier(object, (object).RawField(offset), value); \
    }                                                                \
  } while (false)
#endif

#ifdef V8_DISABLE_WRITE_BARRIERS
#define CONDITIONAL_WEAK_WRITE_BARRIER(object, offset, value, mode)
#else
#define CONDITIONAL_WEAK_WRITE_BARRIER(object, offset, value, mode)           \
  do {                                                                        \
    DCHECK_NOT_NULL(GetHeapFromWritableObject(object));                       \
    DCHECK_NE(mode, UPDATE_EPHEMERON_KEY_WRITE_BARRIER);                      \
    if (mode != SKIP_WRITE_BARRIER) {                                         \
      if (mode == UPDATE_WRITE_BARRIER) {                                     \
        MarkingBarrier(object, (object).RawMaybeWeakField(offset), value);    \
      }                                                                       \
      GenerationalBarrier(object, (object).RawMaybeWeakField(offset), value); \
    }                                                                         \
  } while (false)
#endif

#ifdef V8_DISABLE_WRITE_BARRIERS
#define CONDITIONAL_EPHEMERON_KEY_WRITE_BARRIER(object, offset, value, mode)
#else
#define CONDITIONAL_EPHEMERON_KEY_WRITE_BARRIER(object, offset, value, mode) \
  do {                                                                       \
    DCHECK_NOT_NULL(GetHeapFromWritableObject(object));                      \
    DCHECK_NE(mode, UPDATE_EPHEMERON_KEY_WRITE_BARRIER);                     \
    EphemeronHashTable table = EphemeronHashTable::cast(object);             \
    if (mode != SKIP_WRITE_BARRIER) {                                        \
      if (mode == UPDATE_WRITE_BARRIER) {                                    \
        MarkingBarrier(object, (object).RawField(offset), value);            \
      }                                                                      \
      GenerationalEphemeronKeyBarrier(table, (object).RawField(offset),      \
                                      value);                                \
    }                                                                        \
  } while (false)
#endif

#define ACQUIRE_READ_INT32_FIELD(p, offset) \
  static_cast<int32_t>(base::Acquire_Load(  \
      reinterpret_cast<const base::Atomic32*>(FIELD_ADDR(p, offset))))

#define RELAXED_WRITE_INT8_FIELD(p, offset, value)                             \
  base::Relaxed_Store(reinterpret_cast<base::Atomic8*>(FIELD_ADDR(p, offset)), \
                      static_cast<base::Atomic8>(value));
#define RELAXED_READ_INT8_FIELD(p, offset) \
  static_cast<int8_t>(base::Relaxed_Load(  \
      reinterpret_cast<const base::Atomic8*>(FIELD_ADDR(p, offset))))

#define RELAXED_READ_INT16_FIELD(p, offset) \
  static_cast<int16_t>(base::Relaxed_Load(  \
      reinterpret_cast<const base::Atomic16*>(FIELD_ADDR(p, offset))))

#define RELAXED_WRITE_INT16_FIELD(p, offset, value)             \
  base::Relaxed_Store(                                          \
      reinterpret_cast<base::Atomic16*>(FIELD_ADDR(p, offset)), \
      static_cast<base::Atomic16>(value));

#define RELAXED_READ_UINT32_FIELD(p, offset) \
  static_cast<uint32_t>(base::Relaxed_Load(  \
      reinterpret_cast<const base::Atomic32*>(FIELD_ADDR(p, offset))))

#define RELAXED_WRITE_UINT32_FIELD(p, offset, value)            \
  base::Relaxed_Store(                                          \
      reinterpret_cast<base::Atomic32*>(FIELD_ADDR(p, offset)), \
      static_cast<base::Atomic32>(value));

#define RELAXED_READ_INT32_FIELD(p, offset) \
  static_cast<int32_t>(base::Relaxed_Load(  \
      reinterpret_cast<const base::Atomic32*>(FIELD_ADDR(p, offset))))

#define RELEASE_WRITE_INT32_FIELD(p, offset, value)             \
  base::Release_Store(                                          \
      reinterpret_cast<base::Atomic32*>(FIELD_ADDR(p, offset)), \
      static_cast<base::Atomic32>(value))

#define RELAXED_WRITE_INT32_FIELD(p, offset, value)             \
  base::Relaxed_Store(                                          \
      reinterpret_cast<base::Atomic32*>(FIELD_ADDR(p, offset)), \
      static_cast<base::Atomic32>(value));

#define RELAXED_READ_BYTE_FIELD(p, offset) \
  static_cast<byte>(base::Relaxed_Load(    \
      reinterpret_cast<const base::Atomic8*>(FIELD_ADDR(p, offset))))

#define RELAXED_WRITE_BYTE_FIELD(p, offset, value)                             \
  base::Relaxed_Store(reinterpret_cast<base::Atomic8*>(FIELD_ADDR(p, offset)), \
                      static_cast<base::Atomic8>(value));

#ifdef OBJECT_PRINT
#define DECL_PRINTER(Name) void Name##Print(std::ostream& os);  // NOLINT
#else
#define DECL_PRINTER(Name)
#endif

#ifdef VERIFY_HEAP
#define DECL_VERIFIER(Name) void Name##Verify(Isolate* isolate);
#define EXPORT_DECL_VERIFIER(Name) \
  V8_EXPORT_PRIVATE void Name##Verify(Isolate* isolate);
#else
#define DECL_VERIFIER(Name)
#define EXPORT_DECL_VERIFIER(Name)
#endif

#define DEFINE_DEOPT_ELEMENT_ACCESSORS(name, type) \
  type DeoptimizationData::name() const {          \
    return type::cast(get(k##name##Index));        \
  }                                                \
  void DeoptimizationData::Set##name(type value) { set(k##name##Index, value); }

#define DEFINE_DEOPT_ENTRY_ACCESSORS(name, type)                \
  type DeoptimizationData::name(int i) const {                  \
    return type::cast(get(IndexForEntry(i) + k##name##Offset)); \
  }                                                             \
  void DeoptimizationData::Set##name(int i, type value) {       \
    set(IndexForEntry(i) + k##name##Offset, value);             \
  }

#define TQ_OBJECT_CONSTRUCTORS(Type)               \
 public:                                           \
  constexpr Type() = default;                      \
                                                   \
 protected:                                        \
  template <typename TFieldType, int kFieldOffset> \
  friend class TaggedField;                        \
                                                   \
  inline explicit Type(Address ptr);               \
  friend class TorqueGenerated##Type<Type, Super>;

#define TQ_OBJECT_CONSTRUCTORS_IMPL(Type) \
  inline Type::Type(Address ptr)          \
      : TorqueGenerated##Type<Type, Type::Super>(ptr) {}
