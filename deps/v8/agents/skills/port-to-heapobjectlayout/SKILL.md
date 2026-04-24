---
name: port-to-heapobjectlayout
description: Guide for porting V8 classes from legacy Torque layout to the new C++ HeapObjectLayout. Use when asked to move objects, struct subclasses, or field definitions from Torque to C++ as real members.
---

# Porting V8 Classes to HeapObjectLayout

This skill guides you through porting an arbitrary V8 class from a legacy Torque layout to the new C++ `HeapObjectLayout`. The core idea is to shift layout authority from Torque generation to explicit C++ definitions using specialized layout primitives (like `TaggedMember`), while keeping Torque informed so it can verify the layout and use it in CodeStubAssembler (CSA) and builtins.

## Migration Scope: Inheritance Subtrees

**Crucial Constraint:** A C++ layout object (`HeapObjectLayout` subclass) cannot inherit from a legacy Torque layout object, and vice versa. Because of this, migrations must be done for **entire object inheritance (sub)trees at a time**.

If you are migrating a base class, you must generally migrate all of its subclasses in the same operation. To make large inheritance trees manageable, you can subdivide the tree into smaller subtrees by introducing intermediate C++ layout base classes (e.g., `StructLayout`, `PrimitiveHeapObject`). Once an intermediate base class is migrated (and subclasses updated to inherit from it or its Torque equivalent temporarily), its subclasses can be grouped and migrated in more manageable batches.

## Phase 1: Torque Definition Update (`.tq`)

Torque needs to know that the layout is now managed by C++, but it still needs the field definitions to generate CSA/Builtin offsets and verification assertions.

1. **Locate the class definition** in the relevant `.tq` file.
2. **Add the Layout Annotation:** Above the `extern class` definition, add the `@cppObjectLayoutDefinition` annotation.
3. **Preserve the Fields:** Do **not** remove the field definitions; Torque uses them for verification and offset generation.
4. **Ensure `extern`:** Ensure the class is declared as `extern`.

```torque
// Example: src/objects/my-object.tq
@cppObjectLayoutDefinition
extern class MyObject extends Struct {
  // KEEP these fields here! Torque needs them for layout verification.
  flags: SmiTagged<MyObjectFlags>;
  value: JSAny|TheHole;
  weak_ref: Weak<Map>|Undefined;
}
```

## Phase 2: C++ Header Changes (`.h`)

Define the explicit memory layout in the C++ header.

1. **Include Object Macros:** The file must end with `#include "src/objects/object-macros.h"`.
2. **Class Definition & Macros:** Wrap the class in `V8_OBJECT` and `V8_OBJECT_END`.
3. **Inheritance:** Change the base class to a Layout class (e.g., `StructLayout`, `HeapObjectLayout`, `TrustedObjectLayout`).
4. **Declare Fields (Publicly):** Define the fields matching the Torque definition, appending `_` to their names. Use `TaggedMember` combined with `UnionOf` or `Weak` where appropriate:
   * **Unions:** `TaggedMember<UnionOf<JSAny, Hole>> value_;` (Maps to `JSAny|TheHole`)
   * **Weak Unions:** `TaggedMember<UnionOf<Weak<Map>, Undefined>> weak_ref_;`
   * **Smis:** `TaggedMember<Smi> flags_;`
   * **Doubles:** `UnalignedDoubleMember float_field_;`
   * **External Pointers:** `ExternalPointerMember<kMyTag> ptr_;`
   * **High-Level Types:** For non-tagged fields, prefer storing them as high-level types (e.g., `JSDispatchHandle`, strongly-typed `enum`s) rather than raw low-level types (like `int32_t` or `uint32_t`) whenever conceptually appropriate.
5. **Declare Accessors:** Add inline getters and setters in the `public` section. Return types should match the `UnionOf` types exactly.
   * *Tip:* For complex `UnionOf` types, use a `public` typedef (e.g., `using Value = UnionOf<...>;`) within the class to improve readability of accessors and field declarations.
   * *Note:* You can omit `PtrComprCageBase` getter overloads (e.g. `value(cage_base)`) as `TaggedMember` handles decompression natively.
6. **Diagnostic Declarations:** Add `DECL_PRINTER(MyObject)` and `DECL_VERIFIER(MyObject)`.
7. **Size Constants:** If you need size constants (like `kAlignedSize`), define them as `inline constexpr int` in the header, outside the `V8_OBJECT` block to avoid duplicate symbol errors during linking.

```cpp
// Example: src/objects/my-object.h
#include "src/objects/struct.h"
#include "src/objects/object-macros.h" // Must be the last include

namespace v8::internal {

#include "torque-generated/src/objects/my-object-tq.inc"

V8_OBJECT class MyObject : public StructLayout {
 public:
  // Accessors
  inline int flags() const;
  inline void set_flags(int value);

  inline Tagged<UnionOf<JSAny, Hole>> value() const;
  inline void set_value(Tagged<UnionOf<JSAny, Hole>> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Weak<Map>, Undefined>> weak_ref() const;
  inline void set_weak_ref(Tagged<UnionOf<Weak<Map>, Undefined>> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // GC Body Descriptor
  using BodyDescriptor = StructBodyDescriptor;

  // Diagnostics
  DECL_PRINTER(MyObject)
  DECL_VERIFIER(MyObject)

  // Fields (Public for simplified access and Torque asserts)
  TaggedMember<Smi> flags_;
  TaggedMember<UnionOf<JSAny, Hole>> value_;
  TaggedMember<UnionOf<Weak<Map>, Undefined>> weak_ref_;
} V8_OBJECT_END;

}  // namespace v8::internal
#include "src/objects/object-macros-undef.h"
```

## Phase 3: C++ Inline Header Changes (`-inl.h`)

Implement the accessors using the `TaggedMember` APIs.

```cpp
// Example: src/objects/my-object-inl.h
#include "src/objects/my-object.h"
#include "src/objects/objects-inl.h"
#include "src/objects/object-macros.h"

namespace v8::internal {
#include "torque-generated/src/objects/my-object-tq-inl.inc"

int MyObject::flags() const {
  return flags_.load().value();
}
void MyObject::set_flags(int value) {
  flags_.store(this, Smi::FromInt(value));
}

Tagged<UnionOf<JSAny, Hole>> MyObject::value() const {
  return value_.load();
}
void MyObject::set_value(Tagged<UnionOf<JSAny, Hole>> value, WriteBarrierMode mode) {
  value_.store(this, value, mode);
}

Tagged<UnionOf<Weak<Map>, Undefined>> MyObject::weak_ref() const {
  return weak_ref_.load();
}
void MyObject::set_weak_ref(Tagged<UnionOf<Weak<Map>, Undefined>> value, WriteBarrierMode mode) {
  weak_ref_.store(this, value, mode);
}

}  // namespace v8::internal
#include "src/objects/object-macros-undef.h"
```

### Passing `this` to Write Barriers

If you need to pass the current object to a write barrier macro or function (like `CONDITIONAL_WRITE_BARRIER` or `JS_DISPATCH_HANDLE_WRITE_BARRIER`), it might currently expect a `Tagged<HeapObject>`.

**Do not** cast `this` to a `Tagged` pointer (e.g., `Tagged<MyObject>(this)`). Instead, follow the same overloading advice as with internal APIs: **add an overload** to the underlying write barrier function (e.g., `WriteBarrier::ForJSDispatchHandle`) so that it natively accepts your layout object pointer.

```cpp
// Incorrect (Casting `this`):
JS_DISPATCH_HANDLE_WRITE_BARRIER(Tagged<MyObject>(this), new_handle);
JS_DISPATCH_HANDLE_WRITE_BARRIER(Tagged<HeapObject>(ptr()), new_handle);

// Correct (Add an overload if necessary, then pass `this` directly):
JS_DISPATCH_HANDLE_WRITE_BARRIER(this, new_handle);
```

### Handling Atomic Fields (Acquire/Release)

If your class previously used atomic macros like `DECL_RELAXED_ACCESSORS` or `DECL_ACQUIRE_GETTER`, you can port these directly to `TaggedMember` which provides built-in support for atomic memory orderings:

*   **Acquire Load:** Use `field_.Acquire_Load()`
*   **Release Store:** Use `field_.Release_Store(this, value, mode)`
*   **Relaxed Load:** Use `field_.Relaxed_Load()`
*   **Relaxed Store:** Use `field_.Relaxed_Store(this, value, mode)`

```cpp
// In the .h file
inline Tagged<Object> my_atomic_field(AcquireLoadTag) const;
inline void set_my_atomic_field(Tagged<Object> value, ReleaseStoreTag,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

// In the -inl.h file
Tagged<Object> MyObject::my_atomic_field(AcquireLoadTag) const {
  return my_atomic_field_.Acquire_Load();
}
void MyObject::set_my_atomic_field(Tagged<Object> value, ReleaseStoreTag,
                                   WriteBarrierMode mode) {
  my_atomic_field_.Release_Store(this, value, mode);
}
```

### Missing Functionality in `FooMember` Types

As you migrate classes, you will replace static `FooField` operations (e.g., `TaggedField`, `TrustedPointerField`) with instance-based `FooMember` wrappers (e.g., `TaggedMember`, `TrustedPointerMember`).

If you find that some functionality is missing on a `FooMember` type where it is available on the corresponding `FooField` type, **you should add the missing functionality directly to the `FooMember` class** instead of working around it in your ported class. The implementation of the new `FooMember` method will typically just call into the underlying `FooField` static method.

## Phase 4: Padding and Alignment

V8 object sizes must always be aligned to `kTaggedSize`. When converting from Torque to C++, Torque used to automatically compute and insert padding fields if the object size was uneven (e.g., due to an odd number of 32-bit fields on a 64-bit platform). Now that the layout is explicitly in C++, you must handle this padding manually.

If your fields result in an object size that isn't cleanly divisible by `kTaggedSize` (which is 8 bytes in 64-bit uncompressed builds and 4 bytes otherwise), you must explicitly add an `optional_padding_` field. The `V8_OBJECT` macro automatically applies compiler pragmas (like `-Wpadded`) that will cause a build failure if you miss this, ensuring there are no unintended gaps.

1. **Add Padding Field:** In your class definition in `.h`, use the `TAGGED_SIZE_8_BYTES` macro to add an explicit `uint32_t` padding field if necessary.

```cpp
// Example: src/objects/my-object.h
V8_OBJECT class MyObject : public StructLayout {
 public:
  // ... fields ...
  TaggedMember<Object> value_;
  int32_t some_32_bit_integer_;

#if TAGGED_SIZE_8_BYTES
  // Required because we have a single 32-bit field, making the size
  // end in 4 bytes instead of 8.
  uint32_t optional_padding_;
#endif
} V8_OBJECT_END;
```

*Note: In Torque, this was often written as `@if(TAGGED_SIZE_8_BYTES) optional_padding: uint32;`. You should mirror this in the `.tq` file if you add it to the `.h` file.*

## Phase 5: Diagnostic Implementation (`.cc`)

Manually implement the printer and verifier since Torque no longer generates them.

💡 **Pro-Tip: Steal from Torque!**
Before building with `@cppObjectLayoutDefinition`, look in your build output directory (e.g., `out/x64.debug/gen/torque-generated/src/objects/my-object-tq.cc`). Torque has already written the `MyObjectPrint` and `MyObjectVerify` functions for you. You can simply copy these generated functions and paste them into your manual `.cc` files.

1. **Printer (`src/diagnostics/objects-printer.cc`):**
   *(Copied from generated output and adapted if necessary)*
   ```cpp
   void MyObject::MyObjectPrint(std::ostream& os) {
     PrintHeader(os, "MyObject");
     os << "\n - flags: " << flags();
     os << "\n - value: " << Brief(value());
     os << "\n - weak_ref: " << Brief(weak_ref());
     os << "\n";
   }
   ```
2. **Verifier (`src/diagnostics/objects-debug.cc`):**
   *(Copied from generated output and adapted if necessary)*
   ```cpp
   void MyObject::MyObjectVerify(Isolate* isolate) {
     CHECK(IsMyObject(*this));
     VerifyPointer(isolate, value());
     VerifyMaybeObjectPointer(isolate, weak_ref());
   }
   ```

*Note: Casting is handled by the `Cast` free function in V8's modern object system, so you do not need to manually write `MyObject::Cast(Tagged<Object> object)` methods.*

## Phase 6: Fixing Call Sites and Offsets

Update references to sizes and offsets throughout the codebase (e.g., in `code-stub-assembler.cc` or builtins).

*   **Size:** Use `sizeof(MyObject)` instead of `MyObject::kSize`.
*   **Offsets:** Use `offsetof(MyObject, field_name_)` instead of `MyObject::kFieldNameOffset`. Because the fields are public, `offsetof` will work seamlessly anywhere.

### Handling Field Addresses in Internal APIs
Legacy code often calculated field addresses using `host->field_address(kMyFieldOffset)`. When migrating to `HeapObjectLayout`, **do not** add a generic `field_address(size_t offset)` method to your base layout class.

Instead, prefer passing the field by its actual C++ memory address (e.g., `&host->my_field_`). If an internal API (like `AllocateAndInstallJSDispatchHandle` or `GetJSDispatchTableSpaceFor`) only accepts an `Address` or an `offset`, you should **add a new overload** to that API that accepts a strongly-typed pointer (or `void*`).

```cpp
// Old Approach (Offset-based):
HeapObject::Allocate(host, offsetof(MyObject, field_), isolate, ...);

// New Approach (Pointer-based):
HeapObject::Allocate(host, &host->field_, isolate, ...); // Add an overload for this!
```

```cpp
// Example CodeStubAssembler change:
- TNode<HeapObject> result = Allocate(MyObject::kSize);
- StoreObjectFieldNoWriteBarrier(result, MyObject::kFlagsOffset, zero);
+ TNode<HeapObject> result = Allocate(sizeof(MyObject));
+ StoreObjectFieldNoWriteBarrier(result, offsetof(MyObject, flags_), zero);
```

## Phase 7: BodyDescriptors and `offsetof`

When defining a `BodyDescriptor` for a `HeapObjectLayout` subclass, you often need to use `offsetof` to specify the layout of the newly defined C++ fields. However, using `offsetof` on a class *inside* its own definition results in an "incomplete type" compilation error.

To solve this, V8 offers the `ObjectTraits<T>` pattern. The rule is as follows:

*   **If the `BodyDescriptor` is a legitimate class** (e.g., manually declared with `class BodyDescriptor;` and defined in `objects-body-descriptors-inl.h` because it needs custom iteration logic like `VisitIndirectPointer`):
    You do NOT need `ObjectTraits`. Declare it as a forward declaration inside the class as usual:
    ```cpp
    V8_OBJECT class MyObject : public HeapObjectLayout {
     public:
      // ...
      class BodyDescriptor; // Just a declaration, it's fine!
    } V8_OBJECT_END;
    ```
    *Important note for custom BodyDescriptors:* Do NOT use methods like `RawExternalPointerField(offsetof(Foo, field_))` or offset-based `IterateTrustedPointer` inside `IterateBody`. Instead, use `Slot` constructors that take the address of the member directly, or member-based `Iterate` overloads:
    ```cpp
    // BAD
    IterateTrustedPointer(obj, offsetof(MyObject, pointer_), v, IndirectPointerMode::kStrong, kTag);
    v->VisitExternalPointer(obj, obj->RawExternalPointerField(offsetof(MyObject, ext_), kTag));

    // GOOD
    Tagged<MyObject> my_obj = UncheckedCast<MyObject>(obj);
    IterateTrustedPointer(obj, &my_obj->pointer_, v, IndirectPointerMode::kStrong);
    v->VisitExternalPointer(my_obj, ExternalPointerSlot(&my_obj->ext_, kTag));
    ```
    If the required slot constructor does not exist (e.g. `IndirectPointerSlot` taking a `TrustedPointerMember*`), add it to `slots.h`.

*   **If the `BodyDescriptor` is a typedef** (e.g., aliased using `using BodyDescriptor = FixedBodyDescriptor<...>;` or `SubclassBodyDescriptor<...>;`):
    Do not alias it inside the class body. Instead, define it using the `ObjectTraits` pattern *after* the `V8_OBJECT_END` macro, where the class type is fully complete:
    ```cpp
    V8_OBJECT class MyObject : public HeapObjectLayout {
     public:
      // ... fields ...
    } V8_OBJECT_END;

    template <>
    struct ObjectTraits<MyObject> {
      using BodyDescriptor = FixedBodyDescriptor<offsetof(MyObject, first_field_),
                                                 sizeof(MyObject), sizeof(MyObject)>;
    };
    ```

## Phase 8: Verification

1. **Build:** Run `tools/dev/gm.py`.
2. **Torque Asserts:** If compilation fails in `TorqueGeneratedMyObjectAsserts`, your C++ layout does not match the Torque definition. Fix the ordering or types in your C++ `V8_OBJECT`.
3. **Tests:** Run all test suites (`tools/run-tests.py ... cctest unittests mjsunit`) to confirm the write barriers and offset calculations are functioning perfectly at runtime.

### Trusted Pointer and Code Pointer Accessors

When a class contains `DECL_TRUSTED_POINTER_ACCESSORS(name, Type)` or `DECL_CODE_POINTER_ACCESSORS(name)`, the corresponding C++ macro implementations (`TRUSTED_POINTER_ACCESSORS` and `CODE_POINTER_ACCESSORS`) currently expect `Tagged<HeapObject>` and will fail to compile if used directly on a `HeapObjectLayout` subclass.
To fix this, you must manually implement the accessors using `TrustedCast` and `TrustedPointerMember`:

```cpp
// In the .h file
V8_OBJECT class CodeWrapper : public StructLayout {
 public:
  DECL_CODE_POINTER_ACCESSORS(code)
  // ...
  TrustedPointerMember<Code, kCodeIndirectPointerTag> code_;
} V8_OBJECT_END;

// In the -inl.h file
Tagged<Code> CodeWrapper::code(IsolateForSandbox isolate) const {
  return code_.load(isolate);
}
Tagged<Code> CodeWrapper::code(IsolateForSandbox isolate, AcquireLoadTag tag) const {
  return code_.Acquire_Load(isolate);
}
void CodeWrapper::set_code(Tagged<Code> value, WriteBarrierMode mode) {
  code_.store(this, value, mode);
}
void CodeWrapper::set_code(Tagged<Code> value, ReleaseStoreTag, WriteBarrierMode mode) {
  code_.Release_Store(this, value, mode);
}
bool CodeWrapper::has_code() const {
  return !code_.is_empty();
}
void CodeWrapper::clear_code() {
  code_.clear(this);
}
```

If the object uses a `SubclassBodyDescriptor`, it will NOT automatically visit `TrustedPointerMember`s. You must define a manual `class BodyDescriptor;` and use `IterateTrustedPointer` or `IterateCodePointer` taking the pointer to the member (i.e. `&my_obj->member_`) instead of `StructBodyDescriptor` or `SubclassBodyDescriptor`.
