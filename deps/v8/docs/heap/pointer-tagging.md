# Pointer Tagging and `Tagged<T>` in V8

V8 uses **pointer tagging** to represent both small integers (**Smis**) and references to heap-allocated objects (**HeapObjects**) within a single machine word or compressed 32-bit slot (`Tagged_t`), avoiding separate type header lookups for basic value checks and arithmetic. In C++, raw tagged values are wrapped in the **`Tagged<T>`** static type hierarchy (`src/objects/tagged.h`), paired with a sandbox-aware casting API (`src/objects/casting.h`).

---

## 1. The Tagging Scheme (`src/common/globals.h`)

Because all V8 heap allocations are aligned to at least `kObjectAlignment` (4 bytes when pointer compression is enabled, or 8 bytes on uncompressed 64-bit builds), the least significant bits of any raw heap object address are always `0`. V8 repurposes the lowest two bits of every tagged value as a tag:

| Value Type | Tag Bits (LSB) | Mask | Constant | Description |
| :--- | :--- | :--- | :--- | :--- |
| **Smi** (Small Integer) | `...0` (bit 0 = `0`) | `0x1` (`kSmiTagMask`) | `kSmiTag = 0` | In-place signed integer (no heap allocation). |
| **Strong HeapObject** | `..01` (bits 1..0 = `01`) | `0x3` (`kHeapObjectTagMask`) | `kHeapObjectTag = 1` | Strong reference to a `HeapObject` (`address + 1`). |
| **Weak HeapObject** | `..11` (bits 1..0 = `11`) | `0x3` (`kHeapObjectTagMask`) | `kWeakHeapObjectTag = 3` | Weak reference to a `HeapObject` (`address + 3`). |
| **Cleared Weak Ref** | `0x3` (`..11`) | Full 32-bit value | `kClearedWeakHeapObjectLower32 = 3` | Sentinel stored when the GC clears a weak reference whose target died. |

### Smi Representation
The width and shift of a Smi depend on whether pointer compression (`V8_COMPRESS_POINTERS`) is enabled (`src/common/ptr-compr.h` and `include/v8-internal.h`):

1. **64-bit with Pointer Compression (`SmiValuesAre31Bits() == true`, Default)**:
   * All tagged slots on the heap are 32 bits (`kTaggedSize = 4`).
   * A Smi is a **31-bit signed integer** (`[-2^30, 2^30 - 1]`) shifted left by 1 bit (`kSmiTagSize = 1`, `kSmiShiftSize = 0`).
   * Layout (32-bit `Tagged_t`): `[31-bit signed payload | 0]`.
2. **64-bit without Pointer Compression (`SmiValuesAre32Bits() == true`)**:
   * All tagged slots are 64 bits (`kTaggedSize = 8`).
   * A Smi is a **32-bit signed integer** (`[-2^31, 2^31 - 1]`) shifted left by 32 bits (`kSmiTagSize = 1`, `kSmiShiftSize = 31`).
   * Layout (64-bit `Address`): `[32-bit signed payload | 31 zeros | 0]`.
3. **32-bit Architectures (`ia32`, `arm`)**:
   * Smis are **31-bit signed integers** (`kSmiTagSize = 1`, `kSmiShiftSize = 0`), identical in bit layout to the 32-bit compressed form.

### Untagging and Field Access
Because a strong `HeapObject` pointer has `kHeapObjectTag` (`1`) added to its aligned base address, reading a field at byte `offset` from a tagged pointer `ptr` folds the tag subtraction into the displacement:
```
field_address = ptr - kHeapObjectTag + offset = ptr + (offset - 1)
```
In C++, `HeapObject::address()` (`reinterpret_cast<Address>(this)`) is the **untagged** object address, while `Tagged<HeapObject>::ptr()` holds the **tagged** address (`address() + kHeapObjectTag`).

---

## 2. Dedicated Registers (`kPtrComprCageBaseRegister` & `kRootRegister`)

When pointer compression is enabled on 64-bit architectures, V8 reserves two pinned registers during generated code execution:

| Register Role | C++ Constant | x64 (`src/codegen/x64/register-x64.h`) | ARM64 (`src/codegen/arm64/register-arm64.h`) | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **Root Register** | `kRootRegister` | `r13` | `x26` (`rr`) | Points to the `IsolateData` root bias inside the current `Isolate` for single-instruction access to roots, builtins, and external references. |
| **Cage Base Register** | `kPtrComprCageBaseRegister` | `r14` | `x28` | Holds the 4GB-aligned base address of the main V8 pointer compression cage (`V8HeapCompressionScheme::base()`). |

Decompressing a 32-bit `Tagged_t` into a 64-bit `Address` in JIT code is a single register-plus-zero-extended-32-bit-offset addition against `kPtrComprCageBaseRegister` (`r14` on x64, `x28` on arm64). See [Pointer Compression](pointer-compression.md) for details.

---

## 3. The `Tagged<T>` Static Type System (`src/objects/tagged.h`)

In C++, V8 represents raw tagged pointers using the `Tagged<T>` template class rather than raw `Object*` or `Address`. `Tagged<T>` is a single-word value type (`sizeof(Tagged<T>) == sizeof(Address)`) passed by value in registers.

### Base Classes (`src/objects/tagged-impl.h`)
* **`StrongTaggedBase`** (`TaggedImpl<HeapObjectReferenceType::STRONG, Address>`): Base for all strong `Tagged<T>` values. Provides `IsSmi()`, `IsHeapObject()`, and `ptr()`.
* **`WeakTaggedBase`** (`TaggedImpl<HeapObjectReferenceType::WEAK, Address>`): Base for `Tagged<MaybeObject>` and `Tagged<Weak<T>>`, where bit 1 may be set (`kWeakHeapObjectTag`). Provides `IsSmi()`, `IsWeak()`, `IsWeakOrCleared()`, `IsCleared()`, `GetHeapObject()`, `GetHeapObjectIfStrong()`, and `GetHeapObjectIfWeak()`.

### Core `Tagged<T>` Specializations
* **`Tagged<Smi>`**: Statically known to be a Smi. Provides `.value()` (returning `int`) and `Smi::FromInt(int)` / `Smi::zero()`.
* **`Tagged<HeapObject>`**: Statically known to be a strong `HeapObject`. Provides `operator*` and `operator->` returning `HeapObject&` and `HeapObject*` (where `this` is the **untagged** object address), plus `.address()` (untagged) and `.ptr()` (tagged).
* **`Tagged<T>` (for any `T : public HeapObject`)**: Inherits from `Tagged<Base>` following the C++ inheritance hierarchy of `T`. Provides `operator*` and `operator->` returning `T&` and `T*`, and implicitly upcasts to `Tagged<Base>` (and `Tagged<Object>`).
* **`Tagged<ClearedWeakValue>`**: Represents the cleared weak reference sentinel (`kClearedWeakHeapObjectLower32`).

### Type Unions: `Union<Ts...>` and `UnionOf<Ts...>` (`src/objects/union.h`)
Because JavaScript values and internal slots often hold either a `Smi` or one of several `HeapObject` types, V8 encodes closed sum types at compile time using `Union<Ts...>`:
* **`UnionOf<Ts...>`**: Metafunction that flattens nested unions, deduplicates types, and normalizes `Object` into `Smi, HeapObject` (and `FieldType` into `Smi, Map`). Always construct unions with `UnionOf<...>` rather than raw `Union<...>` unless the type list is already flat and canonical.
* **Implicit Upcasting**: `Tagged<U>` implicitly converts to `Tagged<Union<Ts...>>` if `U` is a subtype of any element in `Ts...` (checked via `is_subtype<U, Union<Ts...>>::value`). A narrower `Tagged<Union<...>>` also implicitly converts to a wider `Tagged<Union<...>>` when every member of the source union is a subtype of the target union.
* **Common Union Aliases (`src/common/globals.h`)**:
  * `Object` (normalized in unions to `Union<Smi, HeapObject>`): Any strong tagged value.
  * `MaybeWeak<T>` = `UnionOf<T, Weak<T>>`: Strong or weak reference to `T`.
  * `HeapObjectReference` = `MaybeWeak<HeapObject>` (`Union<HeapObject, Weak<HeapObject>>`).
  * `MaybeObject` = `MaybeWeak<Object>` (`Union<Smi, HeapObject, Weak<HeapObject>>`): Any strong, weak, or cleared value, or a Smi.
  * `Number` = `Union<Smi, HeapNumber>`
  * `Numeric` = `Union<Smi, HeapNumber, BigInt>`
  * `JSPrimitive` = `Union<Smi, HeapNumber, BigInt, String, Symbol, Boolean, Null, Undefined>`
  * `JSAny` = `UnionOf<JSPrimitive, JSReceiver>`: Any valid JavaScript language value (excluding internal V8 objects like `Hole`, `Map`, or `FixedArray`).
  * `JSAnyNotSmi` = `Union<HeapNumber, BigInt, String, Symbol, Boolean, Null, Undefined, JSReceiver>`
  * `JSPrototype` = `Union<JSReceiver, Null>`

### Weak References: `Tagged<Weak<T>>`
`Weak<T>` is a compile-time marker wrapping a `HeapObject` subtype `T` (it statically rejects `Smi`, `Object`, and `Union` types):
* `Tagged<Weak<T>>` inherits from `WeakTaggedBase` and represents a weak reference (`kWeakHeapObjectTag`) or cleared weak value pointing to an instance of `T`.
* **Conversion Helpers (`src/objects/tagged.h`)**:
  * `MakeWeak(Tagged<T> value)` -> `Tagged<WeakOf<T>>`: Sets the weak tag bit (`ptr | kWeakHeapObjectTag`) on a strong `HeapObject` (statically requires `!is_subtype_v<Smi, T>`).
  * `MakeWeakOrSmi(Tagged<T> value)` -> `Tagged<WeakOf<T>>`: Converts `HeapObject`s to weak references while leaving `Smi` values untouched.
  * `MakeStrong(Tagged<T> value)` -> `Tagged<StrongOf<T>>`: Clears the weak tag bit (`(ptr & ~kWeakHeapObjectTag) | kHeapObjectTag`). **Important**: `MakeStrong` statically asserts `!is_subtype_v<Smi, T>` because clearing bit 1 on a 31-bit Smi would corrupt the Smi's least-significant payload bit.

### Read-Only References: `Tagged<ReadOnly<T>>`
`ReadOnly<T>` is a compile-time marker indicating that a `HeapObject` of type `T` is statically known to live in `ReadOnlySpace`:
* `is_subtype_v<ReadOnly<T>, T>` is `true`, so `Tagged<ReadOnly<T>>` implicitly converts to `Tagged<T>` with zero runtime overhead.
* Conversely, `is_subtype_v<T, ReadOnly<T>>` is `false`: narrowing `Tagged<T>` to `Tagged<ReadOnly<T>>` requires `Cast<ReadOnly<T>>(value)`, which asserts `HeapLayout::InReadOnlySpace(value) && Is<T>(value)`.
* `ReadOnlyRoots` returns `Tagged<ReadOnly<T>>` (for example, `Tagged<ReadOnly<Map>>` or `Tagged<ReadOnly<Oddball>>`), allowing APIs like `HeapObject::HeapObject(Tagged<ReadOnly<Map>>)` to statically enforce that a map is immortal and requires no write barrier.

### `TaggedIndex` (`src/objects/tagged-index.h`)
`Tagged<TaggedIndex>` represents a 31-bit signed integer index that is **always** shifted left by 1 (`kSmiTagSize = 1`) with bit 0 equal to `0` (`kSmiTag`), and **sign-extended to the full 64-bit `Address` width** regardless of whether pointer compression is enabled:
* Unlike a `Smi` (which is shifted by 32 on uncompressed 64-bit builds, or may have uninitialized upper 32 bits when loaded as a 32-bit `Tagged_t` under pointer compression), a `TaggedIndex` has an identical 64-bit representation across all 64-bit build configurations.
* To the garbage collector and Conservative Stack Scanning, a `TaggedIndex` on the stack looks like a Smi (LSB is `0`) and is safely ignored, while generated code can use it directly in 64-bit scaled address calculations without decompression.

---

## 4. The Casting API (`src/objects/casting.h`)

V8 provides a unified set of type predicates and casts that work identically across `Tagged<From>`, `IndirectHandle<From>` (`Handle<From>`), `DirectHandle<From>`, `MaybeIndirectHandle<From>`, and `MaybeDirectHandle<From>`.

| Function | Runtime Check | Sandbox / Static Constraints | When to Use |
| :--- | :--- | :--- | :--- |
| **`Is<T>(value)`** | Evaluates `CastTraits<T>::AllowFrom(value)` (checks tag/strength and `InstanceType` / `Map`). | Allowed on all `Tagged`, `Handle`, and `DirectHandle` types (deleted on `MaybeHandle` / `MaybeDirectHandle`). | Checking whether a value is of type `T` before branching. |
| **`Cast<T>(value)`** | `DCHECK(NullOrIs<T>(value))` in debug builds; zero-cost in release builds. | **Statically forbids downcasting to `TrustedObject`** (`static_assert(!is_subtype_v<To, TrustedObject> \|\| is_subtype_v<From, To>)`). | Standard downcast for in-sandbox objects when the caller knows `value` is a `T`. |
| **`TryCast<T>(value, &out)`** | Runs `Is<T>(value)`; if true, assigns `*out = UncheckedCast<T>(value)` and returns `true`. | If `T` is a `TrustedObject`, `From` must be a subtype of `Union<Smi, Oddball, TrustedObject>` (cannot cast from untrusted `Object`/`HeapObject`). | Conditional narrowing (`if (Tagged<JSArray> arr; TryCast(obj, &arr)) { ... }`). |
| **`UncheckedCast<T>(value)`** | None (raw bit reinterpretation). | No static or runtime checks. | Performance-critical paths right after an explicit check, or during allocation/deserialization before the `Map` word is initialized. |
| **`TrustedCast<T>(value)`** | `DCHECK(NullOrIs<T>(value))` inside a temporary `AllowSandboxAccess` scope (`DCHECK_WITH_SANDBOX_ACCESS_AND_MSG_AND_LOC`). | Permits downcasting to `TrustedObject`. | **Only** when `value` is trusted by construction (e.g., trusted stack frame slots or compiler internals). **Never** use on values read from inside the sandbox. |
| **`SbxCast<T>(value)`** | Always-on `SBXCHECK(NullOrIs<T>(value))` in sandbox builds (crashes safely in release builds on mismatch). | Statically requires `is_subtype_v<To, TrustedObject>` and `is_subtype_v<From, Union<Smi, Oddball, TrustedObject>>`. | Narrowing a `TrustedObject` loaded via a shared `TrustedPointerTable` tag (1:N tag-to-type mapping) where an attacker could swap handles of the same tag. |
| **`CheckedCast<T>(value)`** | Always-on `CHECK(NullOrIs<T>(value))` in both debug and release builds. | Same static rules as `Cast<T>`. | Hardening paths that require an unconditional release assertion outside sandbox-specific `TrustedObject` hierarchies. |
| **`GCSafeCast<T>(obj, heap)`** | `DCHECK(GCAwareObjectTypeCheck<T>(obj, heap))` in debug builds. | Safely follows forwarding pointers in the `MapWord` if `obj` was evacuated during GC. | Inside garbage collector visitors and sweeping/updating passes when `MapWord`s may be forwarded. |

### Why `Cast<T>` Rejects `TrustedObject` Targets
Under the V8 Sandbox (`V8_ENABLE_SANDBOX`), all `TrustedObject` instances (`BytecodeArray`, `Code`, `SharedFunctionInfo::TrustedFunctionData`, `WasmTrustedInstanceData`, etc.) live outside the sandbox in `TrustedSpace`. If C++ code were allowed to write `Cast<BytecodeArray>(untrusted_object)`, a release build would perform no check and treat an attacker-controlled in-sandbox object as a trusted `BytecodeArray`.

Therefore:
1. References from inside the sandbox to `ExposedTrustedObject`s must go through the **`TrustedPointerTable`** (`TrustedPointerMember<T, Tag>`), which returns a type already narrowed to `TrustedTypeFor<Tag>`.
2. If a `TrustedPointerTagRange` maps to a base class shared by multiple `TrustedObject` subclasses (a 1:N mapping), narrowing to a specific subclass must use **`TryCast<SubType>`** or **`SbxCast<SubType>`**, never `Cast<SubType>` or `TrustedCast<SubType>`.
