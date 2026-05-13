// Copyright 2016 the V8 project authors. All rights reserved.  Use of
// this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_OBJECTS_H_
#define V8_WASM_WASM_OBJECTS_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <memory>
#include <optional>

#include "include/v8-wasm.h"
#include "src/base/bit-field.h"
#include "src/debug/interface-types.h"
#include "src/heap/heap.h"
#include "src/objects/backing-store.h"
#include "src/objects/contexts.h"
#include "src/objects/foreign.h"
#include "src/objects/js-function.h"
#include "src/objects/js-objects.h"
#include "src/objects/managed.h"
#include "src/objects/objects-body-descriptors.h"
#include "src/objects/objects.h"
#include "src/objects/struct.h"
#include "src/objects/trusted-object.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/stacks.h"
#include "src/wasm/struct-types.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-module.h"

// Has to be the last include (doesn't have include guards)
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {
namespace wasm {
class NativeModule;
class WasmCode;
struct WasmFunction;
struct WasmGlobal;
class WasmWrapperHandle;
struct WasmModule;
struct WasmTag;
using WasmTagSig = FunctionSig;
class WasmValue;
class WireBytesRef;
}  // namespace wasm

class BreakPoint;
class JSArrayBuffer;
class SeqOneByteString;
class StructBodyDescriptor;
class WasmCapiFunction;
class WasmDispatchTableForImports;
class WasmExceptionTag;
class WasmExportedFunction;
class WasmExternalFunction;
class WasmTrustedInstanceData;
class WasmJSFunction;
class WasmModuleObject;

template <typename CppType>
class Managed;
template <typename CppType>
class TrustedManaged;

#include "torque-generated/src/wasm/wasm-objects-tq.inc"

#define DECL_OPTIONAL_ACCESSORS(name, type) \
  DECL_GETTER(has_##name, bool)             \
  DECL_ACCESSORS(name, type)

namespace wasm {
enum class OnResume : int { kContinue, kThrow };

// Generated "constructor" functions (per the Custom Descriptors proposal)
// store the exported Wasm function they're calling in a context slot.
// We use the first slot in a context with one slot.
static constexpr int kConstructorFunctionContextSlot =
    Context::MIN_CONTEXT_SLOTS;
static constexpr int kConstructorFunctionContextLength =
    kConstructorFunctionContextSlot + 1;

// Same trick for generated method wrappers.
static constexpr int kMethodWrapperContextSlot = Context::MIN_CONTEXT_SLOTS;
static constexpr int kMethodWrapperContextLength =
    kMethodWrapperContextSlot + 1;
}  // namespace wasm

// A helper for an entry for an imported function, indexed statically.
// The underlying storage in the instance is used by generated code to
// call imported functions at runtime.
// Each entry is either:
//   - Wasm to Wrapper, which has fields
//      - object = a WasmImportData
//      - target = entrypoint to import wrapper code
//   - Wasm to Wasm, which has fields
//      - object = target instance data
//      - target = entrypoint for the function
class ImportedFunctionEntry {
 public:
  inline ImportedFunctionEntry(DirectHandle<WasmTrustedInstanceData>,
                               int index);

  // Initialize this entry as a Wasm to Wasm call.
  void SetWasmToWasm(Tagged<WasmTrustedInstanceData> target_instance_object,
                     WasmCodePointer call_target,
                     wasm::CanonicalTypeIndex sig_id
#if V8_ENABLE_DRUMBRAKE
                     ,
                     int exported_function_index
#endif  // V8_ENABLE_DRUMBRAKE
  );

  // Initialize this entry as a Wasm to non-Wasm call, i.e. anything that needs
  // an import wrapper. This accepts the isolate as a parameter since it
  // allocates a WasmImportData.
  V8_EXPORT_PRIVATE void SetWasmToWrapper(
      Isolate*, DirectHandle<JSReceiver> callable,
      std::shared_ptr<wasm::WasmWrapperHandle> wrapper_handle,
      wasm::Suspend suspend, const wasm::CanonicalSig* sig);

  Tagged<JSReceiver> callable();
  Tagged<Object> maybe_callable();
  Tagged<Object> implicit_arg();
  WasmCodePointer target();

#if V8_ENABLE_DRUMBRAKE
  int function_index_in_called_module();
#endif  // V8_ENABLE_DRUMBRAKE

 private:
  DirectHandle<WasmTrustedInstanceData> const instance_data_;
  int const index_;
};

enum InternalizeString : bool { kInternalize = true, kNoInternalize = false };

// Representation of a WebAssembly.Module JavaScript-level object.
V8_OBJECT class WasmModuleObject : public JSObject {
 public:
  using Super = JSObject;

  inline Tagged<Managed<wasm::NativeModule>> managed_native_module() const;
  inline void set_managed_native_module(
      Tagged<Managed<wasm::NativeModule>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Script> script() const;
  inline void set_script(Tagged<Script> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Managed<wasm::NativeModule>::Ptr native_module();

  // Dispatched behavior.
  DECL_PRINTER(WasmModuleObject)
  DECL_VERIFIER(WasmModuleObject)

  class BodyDescriptor;

  static const int kManagedNativeModuleOffsetEnd;
  static const int kScriptOffsetEnd;
  static const int kHeaderSize;

  // Creates a new {WasmModuleObject} for an existing {NativeModule} that is
  // reference counted and might be shared between multiple Isolates.
  V8_EXPORT_PRIVATE static DirectHandle<WasmModuleObject> New(
      Isolate* isolate, std::shared_ptr<wasm::NativeModule> native_module,
      DirectHandle<Script> script);

  // Get the module name, if set. Returns an empty handle otherwise.
  static MaybeDirectHandle<String> GetModuleNameOrNull(
      Isolate*, DirectHandle<WasmModuleObject>);

  // Get the function name of the function identified by the given index.
  // Returns a null handle if the function is unnamed or the name is not a valid
  // UTF-8 string.
  static MaybeDirectHandle<String> GetFunctionNameOrNull(
      Isolate*, DirectHandle<WasmModuleObject>, uint32_t func_index);

  // Get the raw bytes of the function name of the function identified by the
  // given index.
  // Meant to be used for debugging or frame printing.
  // Does not allocate, hence gc-safe.
  base::Vector<const uint8_t> GetRawFunctionName(int func_index);

  // Extract a portion of the wire bytes as UTF-8 string, optionally
  // internalized. (Prefer to internalize early if the string will be used for a
  // property lookup anyway.)
  static DirectHandle<String> ExtractUtf8StringFromModuleBytes(
      Isolate*, base::Vector<const uint8_t> wire_bytes, wasm::WireBytesRef,
      InternalizeString, SharedFlag shared = SharedFlag::kNo);

  TaggedMember<Managed<wasm::NativeModule>> managed_native_module_;
  TaggedMember<Script> script_;
} V8_OBJECT_END;

inline constexpr int WasmModuleObject::kManagedNativeModuleOffsetEnd =
    offsetof(WasmModuleObject, managed_native_module_) + kTaggedSize - 1;
inline constexpr int WasmModuleObject::kScriptOffsetEnd =
    offsetof(WasmModuleObject, script_) + kTaggedSize - 1;
inline constexpr int WasmModuleObject::kHeaderSize = sizeof(WasmModuleObject);

#if V8_ENABLE_SANDBOX || DEBUG
// This should be checked on all code paths that write into WasmDispatchTables.
bool FunctionSigMatchesTable(wasm::CanonicalTypeIndex sig_id,
                             wasm::CanonicalValueType table_type);
#endif

// Representation of a WebAssembly.Table JavaScript-level object.
V8_OBJECT class WasmTableObject : public JSObject {
 public:
  using Super = JSObject;

  class BodyDescriptor;

  inline Tagged<FixedArray> entries() const;
  inline void set_entries(Tagged<FixedArray> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int current_length() const;
  inline void set_current_length(int value);

  inline Tagged<UnionOf<Smi, HeapNumber, BigInt, Undefined>> maximum_length()
      const;
  inline void set_maximum_length(
      Tagged<UnionOf<Smi, HeapNumber, BigInt, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int raw_type() const;
  inline void set_raw_type(int value);

  inline wasm::AddressType address_type() const;
  inline void set_address_type(wasm::AddressType value);

  inline void set_padding_for_address_type_0(uint8_t value) {
    padding_for_address_type_0_ = value;
  }
  inline void set_padding_for_address_type_1(uint16_t value) {
    padding_for_address_type_1_ = value;
  }
#if TAGGED_SIZE_8_BYTES
  inline void set_padding_for_address_type_2(uint32_t value) {
    padding_for_address_type_2_ = value;
  }
#endif  // TAGGED_SIZE_8_BYTES

  inline wasm::ValueType type(const wasm::WasmModule* module);
  inline wasm::CanonicalValueType canonical_type(
      const wasm::WasmModule* module);
  // Use this when you don't care whether the type stored on the in-sandbox
  // object might have been corrupted to contain an invalid type index.
  // That implies that you can't even canonicalize the type!
  inline wasm::ValueType unsafe_type();

  DECL_TRUSTED_POINTER_ACCESSORS(trusted_data, WasmTrustedInstanceData)
  DECL_TRUSTED_POINTER_ACCESSORS(trusted_dispatch_table, WasmDispatchTable)

  DECL_PRINTER(WasmTableObject)
  DECL_VERIFIER(WasmTableObject)

  V8_EXPORT_PRIVATE static int Grow(
      Isolate* isolate, DirectHandle<WasmTableObject> table,
      DirectHandle<WasmDispatchTable> dispatch_table, uint32_t count,
      DirectHandle<Object> init_value);

  // TODO(jkummerow): Consider getting rid of {type}, use {canonical_type}
  // instead.
  V8_EXPORT_PRIVATE static DirectHandle<WasmTableObject> New(
      Isolate* isolate, DirectHandle<WasmTrustedInstanceData> trusted_data,
      wasm::ValueType type, wasm::CanonicalValueType canonical_type,
      uint32_t initial, bool has_maximum, uint64_t maximum,
      DirectHandle<Object> initial_value, wasm::AddressType address_type,
      DirectHandle<WasmDispatchTable>* out_dispatch_table = nullptr);

  inline bool is_in_bounds(uint32_t entry_index);

  inline bool is_table64() const;

  // Get the declared maximum as uint64_t or nullopt if no maximum was declared.
  inline std::optional<uint64_t> maximum_length_u64() const;

  // Thin wrapper around {JsToWasmObject}.
  static MaybeDirectHandle<Object> JSToWasmElement(
      Isolate* isolate, DirectHandle<WasmTableObject> table,
      DirectHandle<Object> entry, const char** error_message);

  // This function will not handle JS objects; i.e., {entry} needs to be in wasm
  // representation.
  V8_EXPORT_PRIVATE static void Set(
      Isolate* isolate, DirectHandle<WasmTableObject> table,
      DirectHandle<WasmDispatchTable> dispatch_table, uint32_t index,
      DirectHandle<Object> entry);

  V8_EXPORT_PRIVATE static DirectHandle<Object> Get(
      Isolate* isolate, DirectHandle<WasmTableObject> table, uint32_t index);

  V8_EXPORT_PRIVATE static void Fill(
      Isolate* isolate, DirectHandle<WasmTableObject> table,
      DirectHandle<WasmDispatchTable> dispatch_table, uint32_t start,
      DirectHandle<Object> entry, uint32_t count);

  static void UpdateDispatchTable(
      Isolate* isolate, DirectHandle<WasmDispatchTable> dispatch_table,
      int entry_index, const wasm::WasmFunction* func,
      DirectHandle<WasmTrustedInstanceData> target_instance
#if V8_ENABLE_DRUMBRAKE
      ,
      int target_func_index
#endif  // V8_ENABLE_DRUMBRAKE
  );
  static void UpdateDispatchTable(
      Isolate* isolate, DirectHandle<WasmDispatchTable> dispatch_table,
      int entry_index, DirectHandle<WasmCapiFunction> capi_function);

  V8_EXPORT_PRIVATE static void SetFunctionTablePlaceholder(
      Isolate* isolate, DirectHandle<WasmTableObject> table, int entry_index,
      DirectHandle<WasmTrustedInstanceData> trusted_instance_data,
      int func_index);

  static const int kCurrentLengthOffsetEnd;
  static const int kTrustedDispatchTableOffsetEnd;
  static const int kTrustedDataOffsetEnd;
  static const int kHeaderSize;

 private:
  // {entry} is either {Null} or a {WasmInternalFunction}.
  static void SetFunctionTableEntry(
      Isolate* isolate, DirectHandle<WasmTableObject> table,
      DirectHandle<WasmDispatchTable> dispatch_table, int entry_index,
      DirectHandle<Object> entry);

 public:
  TaggedMember<FixedArray> entries_;
  TaggedMember<Smi> current_length_;
  TaggedMember<UnionOf<Smi, HeapNumber, BigInt, Undefined>> maximum_length_;
  TaggedMember<Smi> raw_type_;
  TrustedPointerMember<WasmDispatchTable, kWasmDispatchTableIndirectPointerTag>
      trusted_dispatch_table_;
  TrustedPointerMember<WasmTrustedInstanceData,
                       kWasmTrustedInstanceDataIndirectPointerTag>
      trusted_data_;
  uint8_t address_type_;
  uint8_t padding_for_address_type_0_;
  uint16_t padding_for_address_type_1_;
#if TAGGED_SIZE_8_BYTES
  uint32_t padding_for_address_type_2_;
#endif  // TAGGED_SIZE_8_BYTES
} V8_OBJECT_END;

inline constexpr int WasmTableObject::kCurrentLengthOffsetEnd =
    offsetof(WasmTableObject, current_length_) + kTaggedSize - 1;
inline constexpr int WasmTableObject::kTrustedDispatchTableOffsetEnd =
    offsetof(WasmTableObject, trusted_dispatch_table_) + kTrustedPointerSize -
    1;
inline constexpr int WasmTableObject::kTrustedDataOffsetEnd =
    offsetof(WasmTableObject, trusted_data_) + kTrustedPointerSize - 1;
inline constexpr int WasmTableObject::kHeaderSize = sizeof(WasmTableObject);

V8_OBJECT class WasmMemoryMapDescriptor : public JSObject {
 public:
  using Super = JSObject;

  inline Tagged<Weak<HeapObject>> memory() const;
  inline void set_memory(Tagged<Weak<HeapObject>> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int32_t file_descriptor() const;
  inline void set_file_descriptor(int32_t value);

  inline uint32_t offset() const;
  inline void set_offset(uint32_t value);

  inline uint32_t size() const;
  inline void set_size(uint32_t value);

  V8_EXPORT_PRIVATE static MaybeDirectHandle<WasmMemoryMapDescriptor>
  NewFromAnonymous(Isolate* isolate, size_t length);

  V8_EXPORT_PRIVATE static DirectHandle<WasmMemoryMapDescriptor>
  NewFromFileDescriptor(
      Isolate* isolate,
      v8::WasmMemoryMapDescriptor::WasmFileDescriptor file_descriptor);

  // Returns the number of bytes that got mapped into the WebAssembly.Memory.
  V8_EXPORT_PRIVATE size_t MapDescriptor(DirectHandle<WasmMemoryObject> memory,
                                         size_t offset);

  // Returns `false` if an error occurred, otherwise `true`.
  V8_EXPORT_PRIVATE bool UnmapDescriptor();

  DECL_PRINTER(WasmMemoryMapDescriptor)
  DECL_VERIFIER(WasmMemoryMapDescriptor)

  class BodyDescriptor;

  static const int kHeaderSize;

  TaggedMember<Weak<HeapObject>> memory_;
  int32_t file_descriptor_;
  uint32_t offset_;
  uint32_t size_;
#if TAGGED_SIZE_8_BYTES
  uint32_t padding_;
#endif  // TAGGED_SIZE_8_BYTES
} V8_OBJECT_END;

inline constexpr int WasmMemoryMapDescriptor::kHeaderSize =
    sizeof(WasmMemoryMapDescriptor);

// Representation of a WebAssembly.Memory JavaScript-level object.
V8_OBJECT class WasmMemoryObject : public JSObject {
 public:
  using Super = JSObject;

  class BodyDescriptor;

  inline Tagged<UnionOf<JSArrayBuffer, Undefined>> array_buffer() const;
  inline void set_array_buffer(Tagged<UnionOf<JSArrayBuffer, Undefined>> value,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Managed<BackingStore>> managed_backing_store() const;
  inline void set_managed_backing_store(
      Tagged<Managed<BackingStore>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int maximum_pages() const;
  inline void set_maximum_pages(int value);

  inline Tagged<WeakArrayList> instances() const;
  inline void set_instances(Tagged<WeakArrayList> value,
                            WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline wasm::AddressType address_type() const;
  inline void set_address_type(wasm::AddressType value);

  inline void set_padding_for_flags_0(uint8_t value) {
    padding_for_flags_0_ = value;
  }
  inline void set_padding_for_flags_1(uint16_t value) {
    padding_for_flags_1_ = value;
  }
#if TAGGED_SIZE_8_BYTES
  inline void set_padding_for_flags_2(uint32_t value) {
    padding_for_flags_2_ = value;
  }
#endif  // TAGGED_SIZE_8_BYTES

  inline Managed<BackingStore>::Ptr backing_store() const;

  // Add a use of this memory object to the given instance. This updates the
  // internal weak list of instances that use this memory and also updates the
  // fields of the instance to reference this memory's buffer.
  // Note that we update both the non-shared and shared (if any) parts of the
  // instance for faster access to shared memory.
  V8_EXPORT_PRIVATE static void UseInInstance(
      Isolate* isolate, DirectHandle<WasmMemoryObject> memory,
      DirectHandle<WasmTrustedInstanceData> trusted_instance_data,
      DirectHandle<WasmTrustedInstanceData> shared_trusted_instance_data,
      uint32_t memory_index_in_instance);
  inline bool has_maximum_pages();

  inline bool is_memory64() const;

  V8_EXPORT_PRIVATE static DirectHandle<WasmMemoryObject> New(
      Isolate* isolate, MaybeDirectHandle<JSArrayBuffer> maybe_buffer,
      std::shared_ptr<BackingStore> backing_store, int maximum,
      wasm::AddressType address_type);

  V8_EXPORT_PRIVATE static MaybeDirectHandle<WasmMemoryObject> New(
      Isolate* isolate, int initial, int maximum, SharedFlag shared,
      wasm::AddressType address_type);

  // Assign a new (grown) buffer to this memory, also updating the shortcut
  // fields of all instances that use this memory.
  static void SetNewBuffer(Isolate* isolate,
                           DirectHandle<WasmMemoryObject> memory,
                           DirectHandle<JSArrayBuffer> new_buffer);

  // Updates all WebAssembly instances that use this Memory as a memory, after
  // growing or refreshing the memory.
  void UpdateInstances(Isolate* isolate);

  // Fix up a resizable ArrayBuffer that exposes Wasm memory.
  //
  // Usually, an ArrayBuffer is setup from metadata on its BackingStore.
  // However, ABs that are actually WebAssembly memories may have metadata that
  // diverge from their BackingStore's.
  //
  // SABs' is_resizable_by_js may be true even when the BackingStore's
  // equivalent field is false. This happens when there are both growable and
  // non-growable SABs pointing to the same shared WebAssembly memory.
  //
  // AB and SABs' max_byte_length is the requested max passed to the
  // WebAssembly.Memory constructor, while the BackingStore's max is an
  // engine-determined heuristic that may be smaller.
  //
  // Both divergences are impossible for JS-created buffers.
  void FixUpResizableArrayBuffer(Tagged<JSArrayBuffer> new_buffer);

  // Makes a new buffer backed by backing_store and update all the links.
  // The resizability of the new AB is determined by the bit on the backing
  // store, except if `override_resizable` is given (for shared ABs where the
  // bit on the backing store is not authoritative).
  static DirectHandle<JSArrayBuffer> RefreshBuffer(
      Isolate* isolate, DirectHandle<WasmMemoryObject> memory,
      Managed<BackingStore>::Ptr backing_store,
      std::optional<ResizableFlag> override_resizable = {});

  V8_EXPORT_PRIVATE static int32_t Grow(Isolate*,
                                        DirectHandle<WasmMemoryObject>,
                                        uint32_t pages);

  // Returns the current JSArrayBuffer bound to this memory object. If there is
  // none yet it will be allocated and stored in the `array_buffer` field.
  V8_EXPORT_PRIVATE static DirectHandle<JSArrayBuffer> GetArrayBuffer(
      Isolate* isolate, DirectHandle<WasmMemoryObject> memory);

  // Changes resizability of the attached ArrayBuffer.
  // Detaches the existing buffer if it is not shared.
  static DirectHandle<JSArrayBuffer> ChangeArrayBufferResizability(
      Isolate* isolate, DirectHandle<WasmMemoryObject> memory,
      ResizableFlag new_resizability);

  static constexpr int kNoMaximum = -1;

  DECL_PRINTER(WasmMemoryObject)
  DECL_VERIFIER(WasmMemoryObject)

  static const int kHeaderSize;

  TaggedMember<UnionOf<JSArrayBuffer, Undefined>> array_buffer_;
  TaggedMember<Managed<BackingStore>> managed_backing_store_;
  TaggedMember<Smi> maximum_pages_;
  TaggedMember<WeakArrayList> instances_;
  uint8_t address_type_;
  uint8_t padding_for_flags_0_;
  uint16_t padding_for_flags_1_;
#if TAGGED_SIZE_8_BYTES
  uint32_t padding_for_flags_2_;
#endif  // TAGGED_SIZE_8_BYTES
} V8_OBJECT_END;

inline constexpr int WasmMemoryObject::kHeaderSize = sizeof(WasmMemoryObject);

// Representation of a WebAssembly.Global JavaScript-level object.
V8_OBJECT class WasmGlobalObject : public JSObject {
 public:
  using Super = JSObject;

  class BodyDescriptor;

  // We use a ByteArray for non-ref globals and a FixedArray for ref-typed
  // globals.
  using BufferType = Union<ByteArray, FixedArray>;

  DECL_ACCESSORS(buffer, Tagged<BufferType>)

  inline int raw_type() const;
  inline void set_raw_type(int value);

  inline int offset() const;
  inline void set_offset(int value);

  inline int is_mutable() const;
  inline void set_is_mutable(int value);

  DECL_PRIMITIVE_ACCESSORS(unsafe_type, wasm::ValueType)
  DECL_TRUSTED_POINTER_ACCESSORS(trusted_data, WasmTrustedInstanceData)

  // Dispatched behavior.
  DECL_PRINTER(WasmGlobalObject)
  DECL_VERIFIER(WasmGlobalObject)

  V8_EXPORT_PRIVATE static MaybeDirectHandle<WasmGlobalObject> New(
      Isolate* isolate, DirectHandle<WasmTrustedInstanceData> instance_object,
      MaybeDirectHandle<BufferType> maybe_buffer, wasm::ValueType type,
      int32_t offset, bool is_mutable);

  inline int32_t GetI32() const;
  inline int64_t GetI64() const;
  inline float GetF32() const;
  inline double GetF64() const;
  inline uint8_t* GetS128RawBytes() const;
  inline DirectHandle<Object> GetRef() const;

  inline void SetI32(int32_t value);
  inline void SetI64(int64_t value);
  inline void SetF32(float value);
  inline void SetF64(double value);
  // {value} must be an object in Wasm representation.
  inline void SetRef(DirectHandle<Object> value);

  static const int kTrustedDataOffsetEnd;
  static const int kHeaderSize;

  TrustedPointerMember<WasmTrustedInstanceData,
                       kWasmTrustedInstanceDataIndirectPointerTag>
      trusted_data_;
  TaggedMember<BufferType> buffer_;
  TaggedMember<Smi> offset_;
  TaggedMember<Smi> raw_type_;
  TaggedMember<Smi> is_mutable_;

 private:
  // Returns a raw pointer into the buffer where this global's value is stored.
  inline Address storage() const;
} V8_OBJECT_END;

inline constexpr int WasmGlobalObject::kTrustedDataOffsetEnd =
    offsetof(WasmGlobalObject, trusted_data_) + kTrustedPointerSize - 1;
inline constexpr int WasmGlobalObject::kHeaderSize = sizeof(WasmGlobalObject);

class FeedbackConstants {
 public:
  static constexpr int kHeaderSlots = 1;
  static constexpr int kSlotsPerInstruction = 2;
};

// The trusted part of a WebAssembly instance.
// This object lives in trusted space and is never modified from user space.
V8_OBJECT class V8_EXPORT_PRIVATE WasmTrustedInstanceData
    : public ExposedTrustedObject {
 public:
  DECL_OPTIONAL_ACCESSORS(instance_object, Tagged<WasmInstanceObject>)
  DECL_OPTIONAL_ACCESSORS(native_context, Tagged<Context>)
  DECL_ACCESSORS(memory_objects, Tagged<FixedArray>)
#if V8_ENABLE_DRUMBRAKE
  DECL_OPTIONAL_ACCESSORS(interpreter_object, Tagged<Tuple2>)
#endif  // V8_ENABLE_DRUMBRAKE
  // `untagged_globals_buffer`: Storage for non-ref globals.
  DECL_ACCESSORS(untagged_globals_buffer, Tagged<ByteArray>)
  // `tagged_globals_buffer`: Storage for ref-typed globals.
  DECL_ACCESSORS(tagged_globals_buffer, Tagged<FixedArray>)
  // tables: FixedArray of WasmTableObject.
  DECL_OPTIONAL_ACCESSORS(tables, Tagged<FixedArray>)
  DECL_PROTECTED_POINTER_ACCESSORS(dispatch_table_for_imports,
                                   WasmDispatchTableForImports)
  // `imported_mutable_globals_buffers`: Stores (per imported mutable global)
  // the `FixedArray` (for ref-typed globals) or `ByteArray` (non-ref) that
  // stores the global's value (similar to `WasmGlobalObject::buffer`).
  // TODO(clemensb): Merge this with `imported_mutable_globals_offsets` into a
  // single data structure (for faster access).
  DECL_ACCESSORS(imported_mutable_globals_buffers, Tagged<FixedArray>)
  // `imported_mutable_globals`: Stores the offset of the global's value in the
  // buffer stored in `imported_mutable_globals_buffers` (similar to
  // `WasmGlobalObject::offset`).
  // This offset is shifted and scaled to be used in generated code as a direct
  // offset from the buffer's (tagged, uncompressed) pointer.
  DECL_ACCESSORS(imported_mutable_globals_offsets, Tagged<FixedUInt32Array>)
#if V8_ENABLE_DRUMBRAKE
  // Points to an array that contains the function index for each imported Wasm
  // function. This is required to call imported functions from the Wasm
  // interpreter.
  DECL_ACCESSORS(imported_function_indices, Tagged<FixedInt32Array>)
#endif  // V8_ENABLE_DRUMBRAKE
  DECL_PROTECTED_POINTER_ACCESSORS(shared_part, WasmTrustedInstanceData)
  DECL_PROTECTED_POINTER_ACCESSORS(dispatch_table0, WasmDispatchTable)
  DECL_PROTECTED_POINTER_ACCESSORS(dispatch_tables, ProtectedFixedArray)
  inline Tagged<WasmDispatchTable> dispatch_table(uint32_t i) const;
  DECL_OPTIONAL_ACCESSORS(tags_table, Tagged<FixedArray>)
  DECL_ACCESSORS(func_refs, Tagged<FixedArray>)
  DECL_ACCESSORS(managed_object_maps, Tagged<FixedArray>)
  DECL_ACCESSORS(feedback_vectors, Tagged<FixedArray>)
  DECL_ACCESSORS(well_known_imports, Tagged<FixedArray>)
  DECL_PRIMITIVE_ACCESSORS(memory0_start, uint8_t*)
  DECL_PRIMITIVE_ACCESSORS(memory0_size, size_t)
  DECL_PROTECTED_POINTER_ACCESSORS(managed_native_module,
                                   TrustedManaged<wasm::NativeModule>)
  DECL_PRIMITIVE_ACCESSORS(jump_table_start, Address)
  DECL_PRIMITIVE_ACCESSORS(hook_on_function_call_address, Address)
  DECL_PRIMITIVE_ACCESSORS(tiering_budget_array, std::atomic<uint32_t>*)
  DECL_PROTECTED_POINTER_ACCESSORS(memory_bases_and_sizes,
                                   TrustedFixedAddressArray)
  DECL_PROTECTED_POINTER_ACCESSORS(data_segments,
                                   TrustedPodArray<wasm::WireBytesRef>)
  DECL_ACCESSORS(element_segments, Tagged<FixedArray>)
  DECL_PRIMITIVE_ACCESSORS(break_on_entry, uint8_t)

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic. Depending on the V8 build mode there could be no padding.
  inline void clear_padding();

  inline Tagged<WasmMemoryObject> memory_object(int memory_index) const;
  inline uint8_t* memory_base(uint32_t memory_index) const;
  inline size_t memory_size(uint32_t memory_index) const;

  inline wasm::NativeModule* native_module() const;

  inline Tagged<WasmModuleObject> module_object() const;
  inline const wasm::WasmModule* module() const;

  // Dispatched behavior.
  DECL_PRINTER(WasmTrustedInstanceData)
  DECL_VERIFIER(WasmTrustedInstanceData)

// Layout description.
#define FIELD_LIST(V)                                                     \
  /* Often-accessed fields go first to minimize generated code size. */   \
  /* Less than system pointer sized fields come first. */                 \
  V(kProtectedDispatchTable0Offset, kTaggedSize)                          \
  V(kProtectedDispatchTableForImportsOffset, kTaggedSize)                 \
  V(kImportedMutableGlobalsBuffersOffset, kTaggedSize)                    \
  V(kImportedMutableGlobalsOffsetsOffset, kTaggedSize)                    \
  IF_WASM_DRUMBRAKE(V, kImportedFunctionIndicesOffset, kTaggedSize)       \
  /* Optional padding to align system pointer size fields */              \
  V(kOptionalPaddingOffset, POINTER_SIZE_PADDING(kOptionalPaddingOffset)) \
  V(kMemory0StartOffset, kSystemPointerSize)                              \
  V(kMemory0SizeOffset, kSizetSize)                                       \
  V(kJumpTableStartOffset, kSystemPointerSize)                            \
  /* End of often-accessed fields. */                                     \
  /* Continue with system pointer size fields to maintain alignment. */   \
  V(kHookOnFunctionCallAddressOffset, kSystemPointerSize)                 \
  V(kTieringBudgetArrayOffset, kSystemPointerSize)                        \
  /* Less than system pointer size aligned fields are below. */           \
  V(kProtectedMemoryBasesAndSizesOffset, kTaggedSize)                     \
  V(kProtectedDataSegmentsOffset, kTaggedSize)                            \
  V(kElementSegmentsOffset, kTaggedSize)                                  \
  V(kInstanceObjectOffset, kTaggedSize)                                   \
  V(kNativeContextOffset, kTaggedSize)                                    \
  V(kProtectedSharedPartOffset, kTaggedSize)                              \
  V(kMemoryObjectsOffset, kTaggedSize)                                    \
  V(kUntaggedGlobalsBufferOffset, kTaggedSize)                            \
  V(kTaggedGlobalsBufferOffset, kTaggedSize)                              \
  IF_WASM_DRUMBRAKE(V, kInterpreterObjectOffset, kTaggedSize)             \
  V(kTablesOffset, kTaggedSize)                                           \
  V(kProtectedDispatchTablesOffset, kTaggedSize)                          \
  V(kTagsTableOffset, kTaggedSize)                                        \
  V(kFuncRefsOffset, kTaggedSize)                                         \
  V(kManagedObjectMapsOffset, kTaggedSize)                                \
  V(kFeedbackVectorsOffset, kTaggedSize)                                  \
  V(kWellKnownImportsOffset, kTaggedSize)                                 \
  V(kProtectedManagedNativeModuleOffset, kTaggedSize)                     \
  V(kBreakOnEntryOffset, kUInt8Size)                                      \
  /* More padding to make the header pointer-size aligned */              \
  V(kHeaderPaddingOffset, POINTER_SIZE_PADDING(kHeaderPaddingOffset))     \
  V(kHeaderSize, 0)                                                       \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(sizeof(ExposedTrustedObject), FIELD_LIST)
  static_assert(IsAligned(kHeaderSize, kTaggedSize));
  // TODO(ishell, v8:8875): When pointer compression is enabled 8-byte size
  // fields (external pointers, doubles and BigInt data) are only kTaggedSize
  // aligned so checking for alignments of fields bigger than kTaggedSize
  // doesn't make sense until v8:8875 is fixed.
#define ASSERT_FIELD_ALIGNED(offset, size)                                 \
  static_assert(size == 0 || IsAligned(offset, size) ||                    \
                (COMPRESS_POINTERS_BOOL && (size == kSystemPointerSize) && \
                 IsAligned(offset, kTaggedSize)));
  FIELD_LIST(ASSERT_FIELD_ALIGNED)
#undef ASSERT_FIELD_ALIGNED
#undef FIELD_LIST

  // GC support: List all tagged fields and protected fields.
  // V(offset, name)
#define WASM_TAGGED_INSTANCE_DATA_FIELDS(V)                                   \
  V(kInstanceObjectOffset, "instance_object")                                 \
  V(kNativeContextOffset, "native_context")                                   \
  V(kMemoryObjectsOffset, "memory_objects")                                   \
  V(kUntaggedGlobalsBufferOffset, "untagged_globals_buffer")                  \
  V(kTaggedGlobalsBufferOffset, "tagged_globals_buffer")                      \
  V(kImportedMutableGlobalsBuffersOffset, "imported_mutable_globals_buffers") \
  V(kImportedMutableGlobalsOffsetsOffset, "imported_mutable_globals_offsets") \
  IF_WASM_DRUMBRAKE(V, kInterpreterObjectOffset, "interpreter_object")        \
  V(kTablesOffset, "tables")                                                  \
  V(kTagsTableOffset, "tags_table")                                           \
  V(kFuncRefsOffset, "func_refs")                                             \
  V(kManagedObjectMapsOffset, "managed_object_maps")                          \
  V(kFeedbackVectorsOffset, "feedback_vectors")                               \
  V(kWellKnownImportsOffset, "well_known_imports")                            \
  IF_WASM_DRUMBRAKE(V, kImportedFunctionIndicesOffset,                        \
                    "imported_function_indices")                              \
  V(kElementSegmentsOffset, "element_segments")
#define WASM_PROTECTED_INSTANCE_DATA_FIELDS(V)                             \
  V(kProtectedSharedPartOffset, "shared_part")                             \
  V(kProtectedMemoryBasesAndSizesOffset, "memory_bases_and_sizes")         \
  V(kProtectedDataSegmentsOffset, "data_segments")                         \
  V(kProtectedDispatchTable0Offset, "dispatch_table0")                     \
  V(kProtectedDispatchTablesOffset, "dispatch_tables")                     \
  V(kProtectedDispatchTableForImportsOffset, "dispatch_table_for_imports") \
  V(kProtectedManagedNativeModuleOffset, "managed_native_module")

#define WASM_INSTANCE_FIELD_OFFSET(offset, _) offset,
#define WASM_INSTANCE_FIELD_NAME(_, name) name,

#define PLUS_ONE(...) +1
  static constexpr size_t kTaggedFieldsCount =
      WASM_TAGGED_INSTANCE_DATA_FIELDS(PLUS_ONE);
  static constexpr size_t kProtectedFieldsCount =
      WASM_PROTECTED_INSTANCE_DATA_FIELDS(PLUS_ONE);
#undef PLUS_ONE

  static constexpr std::array<uint16_t, kTaggedFieldsCount>
      kTaggedFieldOffsets = {
          WASM_TAGGED_INSTANCE_DATA_FIELDS(WASM_INSTANCE_FIELD_OFFSET)};
  static constexpr std::array<const char*, kTaggedFieldsCount>
      kTaggedFieldNames = {
          WASM_TAGGED_INSTANCE_DATA_FIELDS(WASM_INSTANCE_FIELD_NAME)};
  static constexpr std::array<uint16_t, kProtectedFieldsCount>
      kProtectedFieldOffsets = {
          WASM_PROTECTED_INSTANCE_DATA_FIELDS(WASM_INSTANCE_FIELD_OFFSET)};
  static constexpr std::array<const char*, kProtectedFieldsCount>
      kProtectedFieldNames = {
          WASM_PROTECTED_INSTANCE_DATA_FIELDS(WASM_INSTANCE_FIELD_NAME)};

#undef WASM_INSTANCE_FIELD_OFFSET
#undef WASM_INSTANCE_FIELD_NAME
#undef WASM_TAGGED_INSTANCE_DATA_FIELDS
#undef WASM_PROTECTED_INSTANCE_DATA_FIELDS

  static_assert(kTaggedFieldOffsets.size() == kTaggedFieldNames.size(),
                "every tagged field offset needs a name");
  static_assert(kProtectedFieldOffsets.size() == kProtectedFieldNames.size(),
                "every protected field offset needs a name");

  void SetRawMemory(uint32_t memory_index, uint8_t* mem_start, size_t mem_size);

#if V8_ENABLE_DRUMBRAKE
  // Get the interpreter object associated with the given wasm object.
  // If no interpreter object exists yet, it is created automatically.
  static DirectHandle<Tuple2> GetOrCreateInterpreterObject(
      DirectHandle<WasmInstanceObject>);
  static DirectHandle<Tuple2> GetInterpreterObject(
      DirectHandle<WasmInstanceObject>);
#endif  // V8_ENABLE_DRUMBRAKE

  static DirectHandle<WasmTrustedInstanceData> New(
      Isolate*, DirectHandle<WasmModuleObject>,
      std::shared_ptr<wasm::NativeModule>, SharedFlag shared);

  WasmCodePointer GetCallTarget(uint32_t func_index);

  // Copies table entries. Returns {false} if the ranges are out-of-bounds.
  static bool CopyTableEntries(
      Isolate* isolate,
      DirectHandle<WasmTrustedInstanceData> trusted_instance_data,
      uint32_t table_dst_index, uint32_t table_src_index, uint32_t dst,
      uint32_t src, uint32_t count) V8_WARN_UNUSED_RESULT;

  // Loads a range of elements from element segment into a table.
  // Returns the empty {Optional} if the operation succeeds, or an {Optional}
  // with the error {MessageTemplate} if it fails.
  static std::optional<MessageTemplate> InitTableEntries(
      Isolate* isolate,
      DirectHandle<WasmTrustedInstanceData> trusted_instance_data,
      DirectHandle<WasmTrustedInstanceData> shared_trusted_instance_data,
      uint32_t table_index, uint32_t segment_index, uint32_t dst, uint32_t src,
      uint32_t count) V8_WARN_UNUSED_RESULT;

  class BodyDescriptor;

  // Read a WasmFuncRef from the func_refs FixedArray. Returns true on success
  // and writes the result in the output parameter. Returns false if no func_ref
  // exists yet for this function. Use GetOrCreateFuncRef to always create one.
  bool try_get_func_ref(int index, Tagged<WasmFuncRef>* result);

  // Acquires the {WasmFuncRef} for a given {function_index} from the cache of
  // the given {trusted_instance_data}, or creates a new {WasmInternalFunction}
  // and {WasmFuncRef} if it does not exist yet. The new objects are added to
  // the cache of the {trusted_instance_data} immediately.
  // {precreate_external}: Allocate the corresponding WasmExportedFunction
  // immediately (which is slightly more efficient than letting
  // {WasmInternalFunction::GetOrCreateExternal} do the work separately).
  static DirectHandle<WasmFuncRef> GetOrCreateFuncRef(
      Isolate* isolate,
      DirectHandle<WasmTrustedInstanceData> trusted_instance_data,
      int function_index,
      wasm::PrecreateExternal precreate_external = wasm::kOnlyInternalFunction);

  // Get a raw pointer to the location where the given global is stored.
  Address GetGlobalStorage(const wasm::WasmGlobal&,
                           const DisallowGarbageCollection&);

  // Get the value of a global.
  wasm::WasmValue GetGlobalValue(Isolate*, const wasm::WasmGlobal&);

 private:
  void InitDataSegmentArrays(const wasm::NativeModule*);
} V8_OBJECT_END;

// Representation of a WebAssembly.Instance JavaScript-level object.
// This is mostly a wrapper around the WasmTrustedInstanceData, plus any
// user-set properties.
V8_OBJECT class WasmInstanceObject : public JSObject {
 public:
  using Super = JSObject;

  DECL_TRUSTED_POINTER_ACCESSORS(trusted_data, WasmTrustedInstanceData)

  inline Tagged<WasmModuleObject> module_object() const;
  inline void set_module_object(Tagged<WasmModuleObject> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<JSObject> exports_object() const;
  inline void set_exports_object(Tagged<JSObject> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline const wasm::WasmModule* module() const;

  class BodyDescriptor;

  DECL_PRINTER(WasmInstanceObject)
  DECL_VERIFIER(WasmInstanceObject)

  static const int kTrustedDataOffsetEnd;
  static const int kModuleObjectOffsetEnd;
  static const int kExportsObjectOffsetEnd;
  static const int kHeaderSize;

  TrustedPointerMember<WasmTrustedInstanceData,
                       kWasmTrustedInstanceDataIndirectPointerTag>
      trusted_data_;
  TaggedMember<WasmModuleObject> module_object_;
  TaggedMember<JSObject> exports_object_;
} V8_OBJECT_END;

inline constexpr int WasmInstanceObject::kTrustedDataOffsetEnd =
    offsetof(WasmInstanceObject, trusted_data_) + kTrustedPointerSize - 1;
inline constexpr int WasmInstanceObject::kModuleObjectOffsetEnd =
    offsetof(WasmInstanceObject, module_object_) + kTaggedSize - 1;
inline constexpr int WasmInstanceObject::kExportsObjectOffsetEnd =
    offsetof(WasmInstanceObject, exports_object_) + kTaggedSize - 1;
inline constexpr int WasmInstanceObject::kHeaderSize =
    sizeof(WasmInstanceObject);

// Representation of WebAssembly.Exception JavaScript-level object.
V8_OBJECT class WasmTagObject : public JSObject {
 public:
  using Super = JSObject;

  inline Tagged<HeapObject> tag() const;
  inline void set_tag(Tagged<HeapObject> value,
                      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int canonical_type_index() const;
  inline void set_canonical_type_index(int value);

  class BodyDescriptor;

  DECL_PRINTER(WasmTagObject)
  DECL_VERIFIER(WasmTagObject)

  // Checks whether the given {sig} has the same parameter types as the
  // serialized signature stored within this tag object.
  bool MatchesSignature(wasm::CanonicalTypeIndex expected_index);

  static DirectHandle<WasmTagObject> New(
      Isolate* isolate, const wasm::FunctionSig* sig,
      wasm::CanonicalTypeIndex type_index, DirectHandle<HeapObject> tag,
      DirectHandle<WasmTrustedInstanceData> instance);

  DECL_TRUSTED_POINTER_ACCESSORS(trusted_data, WasmTrustedInstanceData)

  static const int kTagOffsetEnd;
  static const int kHeaderSize;

  TaggedMember<HeapObject> tag_;
  TaggedMember<Smi> canonical_type_index_;
  TrustedPointerMember<WasmTrustedInstanceData,
                       kWasmTrustedInstanceDataIndirectPointerTag>
      trusted_data_;
} V8_OBJECT_END;

inline constexpr int WasmTagObject::kTagOffsetEnd =
    offsetof(WasmTagObject, tag_) + kTaggedSize - 1;
inline constexpr int WasmTagObject::kHeaderSize = sizeof(WasmTagObject);

// Off-heap data object owned by a WasmDispatchTable. Owns the {shared_ptr}s
// which manage the lifetimes of the {WasmWrapperHandle}s.
class WasmDispatchTableData {
 public:
  // If a wrapper is installed at the given {index}, returns the corresponding
  // {WasmWrapperHandle} so that it can be reused in other tables.
  V8_EXPORT_PRIVATE
  std::optional<std::shared_ptr<wasm::WasmWrapperHandle>> MaybeGetWrapperHandle(
      int index) const;

#ifdef DEBUG
  WasmCodePointer WrapperCodePointerForDebugging(int index);
#endif

  void Add(int index, std::shared_ptr<wasm::WasmWrapperHandle> wrapper_handle);
  void Remove(int index);

 private:
  friend class WasmDispatchTable;

  std::unordered_map<int, std::shared_ptr<wasm::WasmWrapperHandle>> wrappers_;
};

// The dispatch table is referenced from a WasmTableObject and from every
// WasmTrustedInstanceData which uses the table. It is used from generated code
// for executing indirect calls.
V8_OBJECT class WasmDispatchTable : public ExposedTrustedObject {
 public:
#if V8_ENABLE_DRUMBRAKE
  static const uint32_t kInvalidFunctionIndex = UINT_MAX;
#endif  // V8_ENABLE_DRUMBRAKE

  // Indicate whether we are modifying an existing entry (for which we might
  // have to update the ref count), or if we are initializing a slot for the
  // first time (in which case we should not read the uninitialized memory).
  enum NewOrExistingEntry : bool { kNewEntry, kExistingEntry };

  class BodyDescriptor;

  static constexpr size_t kLengthOffset = sizeof(ExposedTrustedObject);
  static constexpr size_t kCapacityOffset = kLengthOffset + kUInt32Size;
  static constexpr size_t kProtectedOffheapDataOffset =
      kCapacityOffset + kUInt32Size;
  static constexpr size_t kProtectedUsesOffset =
      kProtectedOffheapDataOffset + kTaggedSize;
  static constexpr size_t kTableTypeOffset = kProtectedUsesOffset + kTaggedSize;
  static constexpr size_t kPaddingSize = TAGGED_SIZE_8_BYTES ? kUInt32Size : 0;
  static constexpr size_t kEntriesOffset =
      kTableTypeOffset + kUInt32Size + kPaddingSize;
  static constexpr size_t kHeaderSize = kEntriesOffset;

  // Entries consist of
  // - target (WasmCodePointer == entry in WasmCodePointerTable),
#if V8_ENABLE_DRUMBRAKE
  // - function_index (uint32_t) (located in place of target pointer),
#endif  // V8_ENABLE_DRUMBRAKE
  // - sig (canonical type index == uint32_t); unused for imports which check
  //   the signature statically,
  // - implicit_arg (protected pointer, tagged sized).
  static constexpr size_t kTargetBias = 0;
#if V8_ENABLE_DRUMBRAKE
  // In jitless mode, reuse the 'target' field storage to hold the (uint32_t)
  // function index.
  static constexpr size_t kFunctionIndexBias = kTargetBias;
#endif  // V8_ENABLE_DRUMBRAKE
  static_assert(sizeof(WasmCodePointer) == kUInt32Size);
  static constexpr size_t kSigBias = kTargetBias + kUInt32Size;
  static constexpr size_t kImplicitArgBias = kSigBias + kUInt32Size;
  static constexpr size_t kEntrySize = kImplicitArgBias + kTaggedSize;

  // Tagged fields must be tagged-size-aligned.
  static_assert(IsAligned(kProtectedOffheapDataOffset, kTaggedSize));
  static_assert(IsAligned(kEntriesOffset, kTaggedSize));
  static_assert(IsAligned(kEntrySize, kTaggedSize));
  static_assert(IsAligned(kImplicitArgBias, kTaggedSize));

  // The total byte size must still fit in an integer.
  static constexpr int kMaxLength = (kMaxInt - kEntriesOffset) / kEntrySize;

  static constexpr int SizeFor(int length) {
    DCHECK_LE(length, kMaxLength);
    return kEntriesOffset + length * kEntrySize;
  }

  static constexpr int OffsetOf(int index) {
    DCHECK_LT(index, kMaxLength);
    return SizeFor(index);
  }

  // The current length of this dispatch table. This is always <= the capacity.
  inline int length() const;
  inline int length(AcquireLoadTag) const;
  // The current capacity. Can be bigger than the current length to allow for
  // more efficient growing.
  inline int capacity() const;

  DECL_PROTECTED_POINTER_ACCESSORS(protected_offheap_data,
                                   TrustedManaged<WasmDispatchTableData>)
  inline WasmDispatchTableData* offheap_data() const;

  // Stores all WasmTrustedInstanceData that refer to this WasmDispatchTable.
  DECL_PROTECTED_POINTER_ACCESSORS(protected_uses, ProtectedWeakFixedArray)

  // Stores the canonical type of the table.
  DECL_PRIMITIVE_ACCESSORS(table_type, wasm::CanonicalValueType)

  // {implicit_arg} will be a WasmImportData, a WasmTrustedInstanceData, or
  // Smi::zero() (if the entry was cleared).
  inline Tagged<Object> implicit_arg(int index) const;
  inline WasmCodePointer target(int index) const;
  inline wasm::CanonicalTypeIndex sig(int index) const;

  // Set an entry for indirect calls that don't go to a WasmToJS wrapper.
  // Wrappers are special since we own the CPT entries for the wrappers.
  // {implicit_arg} has to be a WasmTrustedInstanceData, or Smi::zero() for
  // clearing the slot.
  void V8_EXPORT_PRIVATE SetForNonWrapper(
      int index, Tagged<Union<Smi, WasmTrustedInstanceData>> implicit_arg,
      WasmCodePointer call_target, wasm::CanonicalTypeIndex sig_id,
#if V8_ENABLE_DRUMBRAKE
      uint32_t function_index,
#endif  // V8_ENABLE_DRUMBRAKE
      NewOrExistingEntry new_or_existing);

  // Set an entry for indirect calls to a WasmToJS wrapper.
  void V8_EXPORT_PRIVATE
  SetForWrapper(int index, Tagged<WasmImportData> implicit_arg,
                std::shared_ptr<wasm::WasmWrapperHandle> wrapper_handle,
                wasm::CanonicalTypeIndex sig_id,
#if V8_ENABLE_DRUMBRAKE
                uint32_t function_index,
#endif  // V8_ENABLE_DRUMBRAKE
                NewOrExistingEntry new_or_existing);

#if V8_ENABLE_DRUMBRAKE
  inline uint32_t function_index(int index) const;
#endif  // V8_ENABLE_DRUMBRAKE

  void Clear(int index, NewOrExistingEntry new_or_existing);

  static void V8_EXPORT_PRIVATE
  AddUse(Isolate* isolate, DirectHandle<WasmDispatchTable> dispatch_table,
         DirectHandle<WasmTrustedInstanceData> instance, int table_index);
  // Internal helpers for management of the uses list. These could be factored
  // out into a class similar to WeakArrayList if there are additional use
  // cases for them.
  // The first slot in the list is the used length. After that, we store
  // pairs of <instance, table_index>.
  static Tagged<ProtectedWeakFixedArray> MaybeGrowUsesList(
      Isolate* isolate, DirectHandle<WasmDispatchTable> dispatch_table);

  static V8_WARN_UNUSED_RESULT DirectHandle<WasmDispatchTable> Grow(
      Isolate*, DirectHandle<WasmDispatchTable>, uint32_t new_length);

  DECL_PRINTER(WasmDispatchTable)
  DECL_VERIFIER(WasmDispatchTable)
} V8_OBJECT_END;

V8_OBJECT class WasmDispatchTableForImports : public TrustedObject {
 public:
  class BodyDescriptor;

  static constexpr size_t kLengthOffset = sizeof(TrustedObject);
  static constexpr size_t kPaddingSize = TAGGED_SIZE_8_BYTES ? kUInt32Size : 0;
  static constexpr size_t kProtectedOffheapDataOffset =
      kLengthOffset + kUInt32Size + kPaddingSize;
  static constexpr size_t kEntriesOffset =
      kProtectedOffheapDataOffset + kTaggedSize;

  static constexpr size_t kHeaderSize = kEntriesOffset;

  // Entries consist of
  // - target (WasmCodePointer == entry in WasmCodePointerTable),
#if V8_ENABLE_DRUMBRAKE
  // - function_index (uint32_t) (located in place of target pointer),
#endif  // V8_ENABLE_DRUMBRAKE
  // - implicit_arg (protected pointer, tagged sized).
  static constexpr size_t kTargetBias = 0;
#if V8_ENABLE_DRUMBRAKE
  // In jitless mode, reuse the 'target' field storage to hold the (uint32_t)
  // function index.
  static constexpr size_t kFunctionIndexBias = kTargetBias;
#endif  // V8_ENABLE_DRUMBRAKE
  static constexpr size_t kEntryPaddingSize =
      TAGGED_SIZE_8_BYTES ? kUInt32Size : 0;
  static_assert(sizeof(WasmCodePointer) == kUInt32Size);
  static constexpr size_t kImplicitArgBias =
      kTargetBias + kEntryPaddingSize + kUInt32Size;
  static constexpr size_t kEntrySize = kImplicitArgBias + kTaggedSize;

  // Tagged fields must be tagged-size-aligned.
  static_assert(IsAligned(kProtectedOffheapDataOffset, kTaggedSize));
  static_assert(IsAligned(kEntriesOffset, kTaggedSize));
  static_assert(IsAligned(kEntrySize, kTaggedSize));
  static_assert(IsAligned(kImplicitArgBias, kTaggedSize));

  // The total byte size must still fit in an integer.
  static constexpr int kMaxLength = (kMaxInt - kEntriesOffset) / kEntrySize;

  static constexpr int SizeFor(int length) {
    DCHECK_LE(length, kMaxLength);
    return kEntriesOffset + length * kEntrySize;
  }

  static constexpr int OffsetOf(int index) {
    DCHECK_LT(index, kMaxLength);
    return SizeFor(index);
  }

  // The current length of this dispatch table. This is always <= the capacity.
  inline int length() const;
  inline int length(AcquireLoadTag) const;

  DECL_PROTECTED_POINTER_ACCESSORS(protected_offheap_data,
                                   TrustedManaged<WasmDispatchTableData>)
  inline WasmDispatchTableData* offheap_data() const;

  // {implicit_arg} will be a WasmImportData, a WasmTrustedInstanceData, or
  // Smi::zero() (if the entry was cleared).
  inline Tagged<Object> implicit_arg(int index) const;
  inline WasmCodePointer target(int index) const;

  // Set an entry for indirect calls that don't go to a WasmToJS wrapper.
  // Wrappers are special since we own the CPT entries for the wrappers.
  // {implicit_arg} has to be a WasmTrustedInstanceData, or Smi::zero() for
  // clearing the slot.
  void V8_EXPORT_PRIVATE SetForNonWrapper(
      int index, Tagged<Union<Smi, WasmTrustedInstanceData>> implicit_arg,
      WasmCodePointer call_target, wasm::CanonicalTypeIndex sig_id,
#if V8_ENABLE_DRUMBRAKE
      uint32_t function_index,
#endif  // V8_ENABLE_DRUMBRAKE
      WasmDispatchTable::NewOrExistingEntry new_or_existing);

  // Set an entry for indirect calls to a WasmToJS wrapper.
  void SetForWrapper(int index, Tagged<WasmImportData> implicit_arg,
                     std::shared_ptr<wasm::WasmWrapperHandle> wrapper_handle,
                     wasm::CanonicalTypeIndex sig_id,
#if V8_ENABLE_DRUMBRAKE
                     uint32_t function_index,
#endif  // V8_ENABLE_DRUMBRAKE
                     WasmDispatchTable::NewOrExistingEntry new_or_existing);

  void Clear(int index, WasmDispatchTable::NewOrExistingEntry new_or_existing);

  V8_EXPORT_PRIVATE
  std::optional<std::shared_ptr<wasm::WasmWrapperHandle>> MaybeGetWrapperHandle(
      int index);

  DECL_PRINTER(WasmDispatchTableForImports)
  DECL_VERIFIER(WasmDispatchTableForImports)
} V8_OBJECT_END;

// A Wasm exception that has been thrown out of Wasm code.
class V8_EXPORT_PRIVATE WasmExceptionPackage : public JSObject {
 public:
  static DirectHandle<WasmExceptionPackage> New(
      Isolate* isolate, DirectHandle<WasmExceptionTag> exception_tag,
      int encoded_size);

  static DirectHandle<WasmExceptionPackage> New(
      Isolate* isolate, DirectHandle<WasmExceptionTag> exception_tag,
      DirectHandle<FixedArray> values);

  // The below getters return {undefined} in case the given exception package
  // does not carry the requested values (i.e. is of a different type).
  static DirectHandle<Object> GetExceptionTag(
      Isolate* isolate, DirectHandle<WasmExceptionPackage> exception_package);
  static DirectHandle<Object> GetExceptionValues(
      Isolate* isolate, DirectHandle<WasmExceptionPackage> exception_package);

  // Determines the size of the array holding all encoded exception values.
  static uint32_t GetEncodedSize(const wasm::WasmModule* module,
                                 const wasm::WasmTag* tag);
  static uint32_t GetEncodedSize(const wasm::CanonicalSig* sig);

  // In-object fields.
  enum { kTagIndex, kValuesIndex, kInObjectFieldCount };
  static constexpr int kSize =
      kHeaderSize + (kTaggedSize * kInObjectFieldCount);

  DECL_PRINTER(WasmExceptionPackage)
  DECL_VERIFIER(WasmExceptionPackage)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WasmExceptionPackage);
};

void V8_EXPORT_PRIVATE
EncodeI32ExceptionValue(DirectHandle<FixedArray> encoded_values,
                        uint32_t* encoded_index, uint32_t value);

void V8_EXPORT_PRIVATE
EncodeI64ExceptionValue(DirectHandle<FixedArray> encoded_values,
                        uint32_t* encoded_index, uint64_t value);

void V8_EXPORT_PRIVATE
DecodeI32ExceptionValue(DirectHandle<FixedArray> encoded_values,
                        uint32_t* encoded_index, uint32_t* value);

void V8_EXPORT_PRIVATE
DecodeI64ExceptionValue(DirectHandle<FixedArray> encoded_values,
                        uint32_t* encoded_index, uint64_t* value);

bool UseGenericWasmToJSWrapper(wasm::ImportCallKind kind,
                               const wasm::CanonicalSig* sig,
                               wasm::Suspend suspend);

// A Wasm function that is wrapped and exported to JavaScript.
// Representation of WebAssembly.Function JavaScript-level object.
class WasmExportedFunction : public JSFunction {
 public:
  V8_EXPORT_PRIVATE static bool IsWasmExportedFunction(Tagged<Object> object);

  V8_EXPORT_PRIVATE static DirectHandle<WasmExportedFunction> New(
      Isolate* isolate, DirectHandle<WasmTrustedInstanceData> instance_data,
      DirectHandle<WasmFuncRef> func_ref,
      DirectHandle<WasmInternalFunction> internal_function, int arity,
      DirectHandle<Code> export_wrapper);
  // Compared to the version above, the extra parameters are redundant
  // information, but passing them along from callers that have them readily
  // available is faster than looking them up.
  static DirectHandle<WasmExportedFunction> New(
      Isolate* isolate, DirectHandle<WasmTrustedInstanceData> instance_data,
      DirectHandle<WasmFuncRef> func_ref,
      DirectHandle<WasmInternalFunction> internal_function, int arity,
      DirectHandle<Code> export_wrapper, wasm::ModuleOrigin origin,
      int func_index, wasm::Promise promise);

  // Returns the generic wrapper, or a cached compiled wrapper, or
  // a freshly-compiled wrapper.
  static DirectHandle<Code> GetWrapper(Isolate* isolate,
                                       const wasm::CanonicalSig* sig,
                                       wasm::ModuleOrigin origin);

  // Return a null-terminated string with the debug name in the form
  // 'js-to-wasm:<sig>'.
  static std::unique_ptr<char[]> GetDebugName(const wasm::CanonicalSig* sig);
};

// An external function exposed to Wasm via the C/C++ API.
class WasmCapiFunction : public JSFunction {
 public:
  static bool IsWasmCapiFunction(Tagged<Object> object);

  static DirectHandle<WasmCapiFunction> New(Isolate* isolate,
                                            Address call_target,
                                            DirectHandle<Foreign> embedder_data,
                                            const wasm::CanonicalSig* sig);

  // TODO(clemensb): Remove this accessor.
  const wasm::CanonicalSig* sig() const;

  // Checks whether the given {sig} has the same parameter types as the
  // serialized signature stored within this C-API function object.
  bool MatchesSignature(
      wasm::CanonicalTypeIndex other_canonical_sig_index) const;
};

// Any external function that can be imported/exported in modules. This abstract
// class just dispatches to the following concrete classes:
//  - {WasmExportedFunction}: A proper Wasm function exported from a module.
//  - {WasmCapiFunction}: A function constructed via the C/C++ API.
class WasmExternalFunction : public JSFunction {
 public:
  static bool IsWasmExternalFunction(Tagged<Object> object);

  inline Tagged<WasmFuncRef> func_ref() const;
};

V8_OBJECT class WasmFunctionData : public ExposedTrustedObject {
 public:
  DECL_CODE_POINTER_ACCESSORS(wrapper_code)
  DECL_PROTECTED_POINTER_ACCESSORS(internal, WasmInternalFunction)

  inline Tagged<WasmFuncRef> func_ref() const;
  inline void set_func_ref(Tagged<WasmFuncRef> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int js_promise_flags() const;
  inline void set_js_promise_flags(int value);

  DECL_PRINTER(WasmFunctionData)
  DECL_VERIFIER(WasmFunctionData)

  class BodyDescriptor;

  using SuspendField = base::BitField<wasm::Suspend, 0, 1>;
  using PromiseField = SuspendField::Next<wasm::Promise, 1>;

  static const int kHeaderSize;
  static const int kSize;

 public:
  CodePointerMember wrapper_code_;
  TaggedMember<WasmFuncRef> func_ref_;
  TaggedMember<Smi> js_promise_flags_;
  ProtectedTaggedMember<WasmInternalFunction> protected_internal_;
} V8_OBJECT_END;

inline constexpr int WasmFunctionData::kHeaderSize = sizeof(WasmFunctionData);
inline constexpr int WasmFunctionData::kSize = sizeof(WasmFunctionData);

// Information for a WasmExportedFunction which is referenced as the function
// data of the SharedFunctionInfo underlying the function. For details please
// see the {SharedFunctionInfo::HasWasmExportedFunctionData} predicate.
V8_OBJECT class WasmExportedFunctionData : public WasmFunctionData {
 public:
  DECL_PROTECTED_POINTER_ACCESSORS(instance_data, WasmTrustedInstanceData)
  DECL_CODE_POINTER_ACCESSORS(c_wrapper_code)

  inline int function_index() const;
  inline void set_function_index(int value);

  inline Tagged<Cell> wrapper_budget() const;
  inline void set_wrapper_budget(Tagged<Cell> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int packed_args_size() const;
  inline void set_packed_args_size(int value);

  inline bool is_promising() const;

  // Dispatched behavior.
  DECL_PRINTER(WasmExportedFunctionData)
  DECL_VERIFIER(WasmExportedFunctionData)

  class BodyDescriptor;

  static const int kHeaderSize;
  static const int kSize;

 public:
  ProtectedTaggedMember<WasmTrustedInstanceData> protected_instance_data_;
  TaggedMember<Smi> function_index_;
  TaggedMember<Cell> wrapper_budget_;
  TaggedMember<Smi> packed_args_size_;
  CodePointerMember c_wrapper_code_;
} V8_OBJECT_END;

inline constexpr int WasmExportedFunctionData::kHeaderSize =
    sizeof(WasmExportedFunctionData);
inline constexpr int WasmExportedFunctionData::kSize =
    sizeof(WasmExportedFunctionData);

V8_OBJECT class WasmImportData : public TrustedObject {
 public:
  // Dispatched behavior.
  DECL_PRINTER(WasmImportData)
  DECL_VERIFIER(WasmImportData)

  DECL_PROTECTED_POINTER_ACCESSORS(instance_data, WasmTrustedInstanceData)
  DECL_PROTECTED_POINTER_ACCESSORS(call_origin, TrustedObject)

  inline Tagged<NativeContext> native_context() const;
  inline void set_native_context(Tagged<NativeContext> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSReceiver, Undefined>> callable() const;
  inline void set_callable(Tagged<UnionOf<JSReceiver, Undefined>> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Cell> wrapper_budget() const;
  inline void set_wrapper_budget(Tagged<Cell> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline const wasm::CanonicalSig* sig() const;
  inline void set_sig(const wasm::CanonicalSig* value);

  inline uint32_t bit_field() const;
  inline void set_bit_field(uint32_t value);

  DECL_PRIMITIVE_ACCESSORS(suspend, wasm::Suspend)
  DECL_PRIMITIVE_ACCESSORS(table_slot, uint32_t)

  inline void clear_padding();

  static constexpr int kInvalidCallOrigin = 0;

  void SetIndexInTableAsCallOrigin(Tagged<WasmDispatchTable> table,
                                   int entry_index);
  void SetIndexInTableAsCallOrigin(Tagged<WasmDispatchTableForImports> table,
                                   int entry_index);
  void SetFuncRefAsCallOrigin(Tagged<WasmInternalFunction> func);

  class BodyDescriptor;

  // Usage of the {bit_field()}.
  // "Suspend" is always present.
  using SuspendField = base::BitField<wasm::Suspend, 0, 1>;
  // "TableSlot" is populated when {protected_call_origin} is a
  // {WasmDispatchTable}, and describes the slot in that table.
  static constexpr int kTableSlotBits = 24;
  static_assert(wasm::kV8MaxWasmTableSize < (1u << kTableSlotBits));
  using TableSlotField = SuspendField::Next<uint32_t, kTableSlotBits>;

  static const int kHeaderSize;
  static const int kSize;

 public:
  ProtectedTaggedMember<WasmTrustedInstanceData> protected_instance_data_;
  ProtectedTaggedMember<TrustedObject> protected_call_origin_;
  TaggedMember<NativeContext> native_context_;
  TaggedMember<UnionOf<JSReceiver, Undefined>> callable_;
  TaggedMember<Cell> wrapper_budget_;
  UnalignedValueMember<Address> sig_;
  uint32_t bit_field_;
#if TAGGED_SIZE_8_BYTES
  uint32_t optional_padding_;
#endif
} V8_OBJECT_END;

inline constexpr int WasmImportData::kHeaderSize = sizeof(WasmImportData);
inline constexpr int WasmImportData::kSize = sizeof(WasmImportData);

V8_OBJECT class WasmInternalFunction : public ExposedTrustedObject {
 public:
  // Get the external function if it exists. Returns true and writes to the
  // output parameter if an external function exists. Returns false otherwise.
  bool try_get_external(Tagged<JSFunction>* result);

  V8_EXPORT_PRIVATE static DirectHandle<JSFunction> GetOrCreateExternal(
      DirectHandle<WasmInternalFunction> internal);

  DECL_PROTECTED_POINTER_ACCESSORS(implicit_arg, TrustedObject)

  inline Tagged<UnionOf<JSFunction, Undefined>> external() const;
  inline void set_external(Tagged<UnionOf<JSFunction, Undefined>> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int function_index() const;
  inline void set_function_index(int value);

  inline uint32_t raw_call_target() const;
  inline void set_raw_call_target(uint32_t value);

  inline const wasm::CanonicalSig* sig() const;
  inline void set_sig(const wasm::CanonicalSig* value);

  V8_INLINE WasmCodePointer call_target();
  V8_INLINE void set_call_target(WasmCodePointer code_pointer);

  // Dispatched behavior.
  DECL_PRINTER(WasmInternalFunction)
  DECL_VERIFIER(WasmInternalFunction)

  class BodyDescriptor;

  static const int kHeaderSize;
  static const int kSize;

 public:
  ProtectedTaggedMember<TrustedObject> protected_implicit_arg_;
  TaggedMember<UnionOf<JSFunction, Undefined>> external_;
  TaggedMember<Smi> function_index_;
  uint32_t raw_call_target_;
#if TAGGED_SIZE_8_BYTES
  uint32_t optional_padding_;
#endif  // TAGGED_SIZE_8_BYTES
  UnalignedValueMember<Address> sig_;
} V8_OBJECT_END;

inline constexpr int WasmInternalFunction::kHeaderSize =
    sizeof(WasmInternalFunction);
inline constexpr int WasmInternalFunction::kSize = sizeof(WasmInternalFunction);

V8_OBJECT class WasmFuncRef : public HeapObject {
 public:
  inline Tagged<WasmInternalFunction> internal(IsolateForSandbox isolate) const;
  inline Tagged<WasmInternalFunction> internal(IsolateForSandbox isolate,
                                               AcquireLoadTag) const;
  inline void set_internal(Tagged<WasmInternalFunction> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set_internal(Tagged<WasmInternalFunction> value, ReleaseStoreTag,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline bool has_internal() const;
  inline void clear_internal();

  DECL_PRINTER(WasmFuncRef)
  DECL_VERIFIER(WasmFuncRef)

  class BodyDescriptor;

  static const int kHeaderSize;
  static const int kSize;

  TrustedPointerMember<WasmInternalFunction,
                       kWasmInternalFunctionIndirectPointerTag>
      trusted_internal_;
} V8_OBJECT_END;

inline constexpr int WasmFuncRef::kHeaderSize = sizeof(WasmFuncRef);
inline constexpr int WasmFuncRef::kSize = sizeof(WasmFuncRef);

V8_OBJECT class WasmCapiFunctionData : public WasmFunctionData {
 public:
  inline Tagged<Foreign> embedder_data() const;
  inline void set_embedder_data(Tagged<Foreign> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(WasmCapiFunctionData)
  DECL_VERIFIER(WasmCapiFunctionData)

  class BodyDescriptor;

  static const int kHeaderSize;
  static const int kSize;

 public:
  TaggedMember<Foreign> embedder_data_;
} V8_OBJECT_END;

inline constexpr int WasmCapiFunctionData::kHeaderSize =
    sizeof(WasmCapiFunctionData);
inline constexpr int WasmCapiFunctionData::kSize = sizeof(WasmCapiFunctionData);

V8_OBJECT class WasmResumeData : public HeapObject {
 public:
  inline Tagged<WasmSuspenderObject> trusted_suspender(
      IsolateForSandbox isolate) const;
  inline Tagged<WasmSuspenderObject> trusted_suspender(
      IsolateForSandbox isolate, AcquireLoadTag) const;
  inline void set_trusted_suspender(
      Tagged<WasmSuspenderObject> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set_trusted_suspender(
      Tagged<WasmSuspenderObject> value, ReleaseStoreTag,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline bool has_trusted_suspender() const;
  inline void clear_trusted_suspender();

  inline int on_resume() const;
  inline void set_on_resume(int value);

  DECL_PRINTER(WasmResumeData)
  DECL_VERIFIER(WasmResumeData)

  class BodyDescriptor;

  static const int kHeaderSize;
  static const int kSize;

  TrustedPointerMember<WasmSuspenderObject, kWasmSuspenderIndirectPointerTag>
      trusted_suspender_;
  TaggedMember<Smi> on_resume_;
} V8_OBJECT_END;

inline constexpr int WasmResumeData::kHeaderSize = sizeof(WasmResumeData);
inline constexpr int WasmResumeData::kSize = sizeof(WasmResumeData);

class WasmScript : public AllStatic {
 public:
  // Position used for storing "on entry" breakpoints (a.k.a. instrumentation
  // breakpoints). This would be an illegal position for any other breakpoint.
  static constexpr int kOnEntryBreakpointPosition = -1;

  // Set a breakpoint on the given byte position inside the given module.
  // This will affect all live and future instances of the module.
  // The passed position might be modified to point to the next breakable
  // location inside the same function.
  // If it points outside a function, or behind the last breakable location,
  // this function returns false and does not set any breakpoint.
  V8_EXPORT_PRIVATE static bool SetBreakPoint(
      DirectHandle<Script>, int* position,
      DirectHandle<BreakPoint> break_point);

  // Set an "on entry" breakpoint (a.k.a. instrumentation breakpoint) inside
  // the given module. This will affect all live and future instances of the
  // module.
  V8_EXPORT_PRIVATE static void SetInstrumentationBreakpoint(
      DirectHandle<Script>, DirectHandle<BreakPoint> break_point);

  // Set a breakpoint on first breakable position of the given function index
  // inside the given module. This will affect all live and future instances of
  // the module.
  V8_EXPORT_PRIVATE static bool SetBreakPointOnFirstBreakableForFunction(
      DirectHandle<Script>, int function_index,
      DirectHandle<BreakPoint> break_point);

  // Set a breakpoint at the breakable offset of the given function index
  // inside the given module. This will affect all live and future instances of
  // the module.
  V8_EXPORT_PRIVATE static bool SetBreakPointForFunction(
      DirectHandle<Script>, int function_index, int breakable_offset,
      DirectHandle<BreakPoint> break_point);

  // Remove a previously set breakpoint at the given byte position inside the
  // given module. If this breakpoint is not found this function returns false.
  V8_EXPORT_PRIVATE static bool ClearBreakPoint(
      DirectHandle<Script>, int position, DirectHandle<BreakPoint> break_point);

  // Remove a previously set breakpoint by id. If this breakpoint is not found,
  // returns false.
  V8_EXPORT_PRIVATE static bool ClearBreakPointById(DirectHandle<Script>,
                                                    int breakpoint_id);

  // Remove all set breakpoints.
  static void ClearAllBreakpoints(Tagged<Script>);

  // Get a list of all possible breakpoints within a given range of this module.
  V8_EXPORT_PRIVATE static bool GetPossibleBreakpoints(
      const wasm::NativeModule* native_module, const debug::Location& start,
      const debug::Location& end, std::vector<debug::BreakLocation>* locations);

  // Return an empty handle if no breakpoint is hit at that location, or a
  // FixedArray with all hit breakpoint objects.
  static MaybeDirectHandle<FixedArray> CheckBreakPoints(
      Isolate*, DirectHandle<Script>, int position,
      StackFrameId stack_frame_id);

 private:
  // Helper functions that update the breakpoint info list.
  static void AddBreakpointToInfo(DirectHandle<Script>, int position,
                                  DirectHandle<BreakPoint> break_point);
};

// Tags provide an object identity for each exception defined in a wasm module
// header. They are referenced by the following fields:
//  - {WasmTagObject::tag}: The tag of the {Tag} object.
//  - {WasmInstanceObject::tags_table}: List of tags used by an instance.
V8_OBJECT class WasmExceptionTag : public Struct {
 public:
  V8_EXPORT_PRIVATE static DirectHandle<WasmExceptionTag> New(Isolate* isolate,
                                                              int index);

  inline int index() const;
  inline void set_index(int value);

  using BodyDescriptor = StructBodyDescriptor;

  DECL_PRINTER(WasmExceptionTag)
  DECL_VERIFIER(WasmExceptionTag)

 private:
  friend class TorqueGeneratedWasmExceptionTagAsserts;

  TaggedMember<Smi> index_;
} V8_OBJECT_END;

// Data annotated to the asm.js Module function. Used for later instantiation of
// that function.
V8_OBJECT class AsmWasmData : public ExposedTrustedObject {
 public:
  static Handle<AsmWasmData> New(
      Isolate* isolate, std::shared_ptr<wasm::NativeModule> native_module,
      uint64_t uses_bitset);

  inline Tagged<TrustedManaged<wasm::NativeModule>> managed_native_module()
      const;
  inline void set_managed_native_module(
      Tagged<TrustedManaged<wasm::NativeModule>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline bool has_managed_native_module() const;
  inline void clear_managed_native_module();

  inline uint64_t uses_bitset() const;
  inline void set_uses_bitset(uint64_t value);

  DECL_PRINTER(AsmWasmData)
  DECL_VERIFIER(AsmWasmData)

  class BodyDescriptor;

 private:
  friend class TorqueGeneratedAsmWasmDataAsserts;

  ProtectedTaggedMember<TrustedManaged<wasm::NativeModule>>
      managed_native_module_;
  UnalignedValueMember<uint64_t> uses_bitset_;
} V8_OBJECT_END;

V8_OBJECT class WasmTypeInfo : public HeapObject {
 public:
  inline uint32_t canonical_type() const;
  inline void set_canonical_type(uint32_t value);

  inline uint32_t canonical_element_type() const;
  inline void set_canonical_element_type(uint32_t value);

  inline int supertypes_length() const;
  inline void set_supertypes_length(int value);

  inline Tagged<Object> supertypes(int i) const;
  inline void set_supertypes(int i, Tagged<Object> value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline wasm::CanonicalValueType type() const;
  inline wasm::CanonicalTypeIndex type_index() const;
  inline wasm::CanonicalValueType element_type() const;  // Only for WasmArrays.

  DECL_PRINTER(WasmTypeInfo)
  DECL_VERIFIER(WasmTypeInfo)

  class BodyDescriptor;

  static constexpr int SizeFor(int supertypes_length);
  inline int AllocatedSize() const;

  static const int kSupertypesOffset;
  static const int kHeaderSize;

  uint32_t canonical_type_;
  uint32_t canonical_element_type_;
  TaggedMember<Smi> supertypes_length_;
  FLEXIBLE_ARRAY_MEMBER(TaggedMember<Object>, supertypes);
} V8_OBJECT_END;

inline constexpr int WasmTypeInfo::kSupertypesOffset =
    OFFSET_OF_DATA_START(WasmTypeInfo);
inline constexpr int WasmTypeInfo::kHeaderSize = kSupertypesOffset;

constexpr int WasmTypeInfo::SizeFor(int supertypes_length) {
  return kSupertypesOffset + supertypes_length * kTaggedSize;
}

// WasmObject is the abstract base for Wasm GC data ref types (WasmStruct and
// WasmArray). It carries no fields of its own; subclasses lay out their
// payload directly after the JSReceiver header.
V8_OBJECT class WasmObject : public JSReceiver {
 public:
  static const int kHeaderSize;

 protected:
  // Returns boxed value of the object's field/element with given type and
  // offset.
  static inline DirectHandle<Object> ReadValueAt(Isolate* isolate,
                                                 DirectHandle<HeapObject> obj,
                                                 wasm::CanonicalValueType type,
                                                 uint32_t offset);
} V8_OBJECT_END;

inline constexpr int WasmObject::kHeaderSize = sizeof(WasmObject);

V8_OBJECT class WasmStruct : public WasmObject {
 public:
  static const wasm::CanonicalStructType* GcSafeType(Tagged<Map> map);
  static inline int Size(const wasm::StructType* type);
  static inline int Size(const wasm::CanonicalStructType* type);
  static inline int GcSafeSize(Tagged<Map> map);
  static inline void EncodeInstanceSizeInMap(int instance_size,
                                             Tagged<Map> map);
  static inline int DecodeInstanceSizeFromMap(Tagged<Map> map);

  // Returns the address of the field at given offset.
  inline Address RawFieldAddress(int raw_offset);

  // Returns the ObjectSlot for tagged value at given offset.
  inline ObjectSlot RawField(int raw_offset);

  // Only for structs whose type describes another type.
  static DirectHandle<WasmStruct> AllocateDescriptorUninitialized(
      Isolate* isolate, DirectHandle<WasmTrustedInstanceData> trusted_data,
      wasm::ModuleTypeIndex index, DirectHandle<Map> map,
      DirectHandle<Object> first_field);
  inline Tagged<Map> described_rtt() const;
  inline void set_described_rtt(Tagged<Map> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  V8_EXPORT_PRIVATE wasm::WasmValue GetFieldValue(uint32_t field_index);
  inline void SetTaggedFieldValue(int raw_offset, Tagged<Object> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(WasmStruct)
  DECL_VERIFIER(WasmStruct)

  class BodyDescriptor;

  static const int kHeaderSize;
} V8_OBJECT_END;

inline constexpr int WasmStruct::kHeaderSize = sizeof(WasmStruct);

int WasmStruct::Size(const wasm::StructType* type) {
  // Object size must fit into a Smi (because of filler objects), and its
  // computation must not overflow.
  static_assert(Smi::kMaxValue <= kMaxInt);
  DCHECK_LE(type->total_fields_size(), Smi::kMaxValue - kHeaderSize);
  return std::max(kHeaderSize + static_cast<int>(type->total_fields_size()),
                  Heap::kMinObjectSizeInTaggedWords * kTaggedSize);
}

int WasmStruct::Size(const wasm::CanonicalStructType* type) {
  // Object size must fit into a Smi (because of filler objects), and its
  // computation must not overflow.
  static_assert(Smi::kMaxValue <= kMaxInt);
  DCHECK_LE(type->total_fields_size(), Smi::kMaxValue - kHeaderSize);
  return std::max(kHeaderSize + static_cast<int>(type->total_fields_size()),
                  Heap::kMinObjectSizeInTaggedWords * kTaggedSize);
}

V8_OBJECT class WasmArray : public WasmObject {
 public:
  inline uint32_t length() const;
  inline void set_length(uint32_t value);

  static inline const wasm::CanonicalValueType GcSafeElementType(
      Tagged<Map> map);

  // Get the {ObjectSlot} corresponding to the element at {index}. Requires that
  // this is a reference array.
  inline ObjectSlot ElementSlot(uint32_t index);
  V8_EXPORT_PRIVATE wasm::WasmValue GetElement(uint32_t index);

  static inline int SizeFor(Tagged<Map> map, int length);
  static constexpr int SizeFor(int element_size, int length);

  // Returns boxed value of the array's element.
  static inline DirectHandle<Object> GetElement(Isolate* isolate,
                                                DirectHandle<WasmArray> array,
                                                uint32_t index);

  void SetTaggedElement(uint32_t index, DirectHandle<Object> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Returns the offset/Address of the element at {index}.
  inline uint32_t element_offset(uint32_t index);
  inline Address ElementAddress(uint32_t index);

  static constexpr int MaxLength(uint32_t element_size_bytes) {
    // The total object size must fit into a Smi, for filler objects. To make
    // the behavior of Wasm programs independent from the Smi configuration,
    // we hard-code the smaller of the two supported ranges.
    return RoundDown(
        (int{SmiTagging<4>::kSmiMaxValue} - kHeaderSize) / element_size_bytes,
        kTaggedSize);
  }

  static int MaxLength(const wasm::ArrayType* type) {
    return MaxLength(type->element_type().value_kind_size());
  }

  static inline void EncodeElementSizeInMap(int element_size, Tagged<Map> map);
  static inline int DecodeElementSizeFromMap(Tagged<Map> map);

  DECL_PRINTER(WasmArray)
  DECL_VERIFIER(WasmArray)

  class BodyDescriptor;

  static const int kHeaderSize;

  uint32_t length_;
#if TAGGED_SIZE_8_BYTES
  uint32_t optional_padding_;
#endif
} V8_OBJECT_END;

inline constexpr int WasmArray::kHeaderSize = sizeof(WasmArray);

// The suspender object provides an API to suspend and resume wasm code using
// promises. See: https://github.com/WebAssembly/js-promise-integration.
V8_OBJECT class WasmSuspenderObject : public ExposedTrustedObject {
 public:
  enum State : int { kInactive = 0, kActive, kSuspended };
  DECL_EXTERNAL_POINTER_ACCESSORS(stack, wasm::StackMemory*)
  DECL_PROTECTED_POINTER_ACCESSORS(parent, WasmSuspenderObject)

  inline Tagged<UnionOf<JSPromise, Undefined>> promise() const;
  inline void set_promise(Tagged<UnionOf<JSPromise, Undefined>> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSObject, Undefined>> resume() const;
  inline void set_resume(Tagged<UnionOf<JSObject, Undefined>> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSObject, Undefined>> reject() const;
  inline void set_reject(Tagged<UnionOf<JSObject, Undefined>> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(WasmSuspenderObject)
  DECL_VERIFIER(WasmSuspenderObject)

  class BodyDescriptor;

  static const int kHeaderSize;
  static const int kSize;

 public:
  ExternalPointerMember<kWasmStackMemoryTag> stack_;
  ProtectedTaggedMember<WasmSuspenderObject> parent_;
  TaggedMember<UnionOf<JSPromise, Undefined>> promise_;
  TaggedMember<UnionOf<JSObject, Undefined>> resume_;
  TaggedMember<UnionOf<JSObject, Undefined>> reject_;
} V8_OBJECT_END;

inline constexpr int WasmSuspenderObject::kHeaderSize =
    sizeof(WasmSuspenderObject);
inline constexpr int WasmSuspenderObject::kSize = sizeof(WasmSuspenderObject);

V8_OBJECT class WasmSuspendingObject : public JSObject {
 public:
  inline Tagged<JSReceiver> callable() const;
  inline void set_callable(Tagged<JSReceiver> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  V8_EXPORT_PRIVATE static DirectHandle<WasmSuspendingObject> New(
      Isolate* isolate, DirectHandle<JSReceiver> callable);
  DECL_PRINTER(WasmSuspendingObject)
  DECL_VERIFIER(WasmSuspendingObject)

  class BodyDescriptor;

  static const int kHeaderSize;

  TaggedMember<JSReceiver> callable_;
} V8_OBJECT_END;

inline constexpr int WasmSuspendingObject::kHeaderSize =
    sizeof(WasmSuspendingObject);

// The continuation object is a token used during resume & suspend
// See: https://github.com/WebAssembly/stack-switching.
V8_OBJECT class WasmContinuationObject : public HeapObject {
 public:
  inline Tagged<WasmStackObject> stack_obj() const;
  inline void set_stack_obj(Tagged<WasmStackObject> value,
                            WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(WasmContinuationObject)
  DECL_VERIFIER(WasmContinuationObject)

  using BodyDescriptor = StructBodyDescriptor;

  static const int kHeaderSize;
  static const int kSize;

  TaggedMember<WasmStackObject> stack_obj_;
} V8_OBJECT_END;

inline constexpr int WasmContinuationObject::kHeaderSize =
    sizeof(WasmContinuationObject);
inline constexpr int WasmContinuationObject::kSize =
    sizeof(WasmContinuationObject);

V8_OBJECT class WasmStackObject : public HeapObject {
 public:
  DECL_EXTERNAL_POINTER_ACCESSORS(stack, wasm::StackMemory*)

  DECL_PRINTER(WasmStackObject)
  DECL_VERIFIER(WasmStackObject)

  class BodyDescriptor;

  static const int kHeaderSize;
  static const int kSize;

  ExternalPointerMember<kWasmStackMemoryTag> stack_;
} V8_OBJECT_END;

inline constexpr int WasmStackObject::kHeaderSize = sizeof(WasmStackObject);
inline constexpr int WasmStackObject::kSize = sizeof(WasmStackObject);

V8_OBJECT class WasmFastApiCallData : public HeapObject {
 public:
  inline Tagged<HeapObject> signature() const;
  inline void set_signature(Tagged<HeapObject> value,
                            WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> callback_data() const;
  inline void set_callback_data(Tagged<Object> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<MaybeObject> cached_map() const;
  inline void set_cached_map(Tagged<MaybeObject> value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(WasmFastApiCallData)
  DECL_VERIFIER(WasmFastApiCallData)

  class BodyDescriptor;

  static constexpr int SizeFor() { return sizeof(WasmFastApiCallData); }


  TaggedMember<HeapObject> signature_;
  TaggedMember<Object> callback_data_;
  TaggedMember<MaybeObject> cached_map_;
} V8_OBJECT_END;


V8_OBJECT class WasmStringViewIter : public HeapObject {
 public:
  inline Tagged<String> string() const;
  inline void set_string(Tagged<String> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline uint32_t offset() const;
  inline void set_offset(uint32_t value);

  DECL_PRINTER(WasmStringViewIter)
  DECL_VERIFIER(WasmStringViewIter)

  class BodyDescriptor;

  static constexpr int SizeFor() { return sizeof(WasmStringViewIter); }

  TaggedMember<String> string_;
  uint32_t offset_;
#if TAGGED_SIZE_8_BYTES
  uint32_t optional_padding_;
#endif  // TAGGED_SIZE_8_BYTES
} V8_OBJECT_END;

V8_OBJECT class WasmNull : public HeapObject {
 public:
#if V8_STATIC_ROOTS_BOOL || V8_STATIC_ROOTS_GENERATION_BOOL
  // TODO(manoskouk): Make it smaller if able and needed.
  static constexpr int kPayloadSize = 64 * KB;
  // Payload should be a multiple of page size.
  static_assert(kPayloadSize % kMinimumOSPageSize == 0);

  Address payload() { return reinterpret_cast<Address>(this) + kPayloadOffset; }

  static const int kPayloadOffset;
  static const int kSizeWithPayload;
#endif

  static const int kHeaderSize;
  static const int kSize;

  DECL_PRINTER(WasmNull)
  DECL_VERIFIER(WasmNull)

  // WasmNull cannot use `FixedBodyDescriptorFor()` as its map is variable size
  // (not fixed size) as kSize is too large for a fixed-size map.
  class BodyDescriptor;
} V8_OBJECT_END;

inline constexpr int WasmNull::kHeaderSize = sizeof(WasmNull);
#if V8_STATIC_ROOTS_BOOL || V8_STATIC_ROOTS_GENERATION_BOOL
inline constexpr int WasmNull::kPayloadOffset = WasmNull::kHeaderSize;
inline constexpr int WasmNull::kSizeWithPayload =
    WasmNull::kPayloadOffset + WasmNull::kPayloadSize;
inline constexpr int WasmNull::kSize = WasmNull::kSizeWithPayload;
// Any wasm struct offset should fit in the object.
static_assert(WasmNull::kSizeWithPayload >=
              WasmStruct::kHeaderSize +
                  (wasm::kMaxStructFieldIndexForImplicitNullCheck + 1) *
                      kSimd128Size);
#else
inline constexpr int WasmNull::kSize = WasmNull::kHeaderSize;
#endif

#undef DECL_OPTIONAL_ACCESSORS

// If {opt_native_context} is not null, creates a contextful map bound to
// that context; otherwise creates a context-independent map (which must then
// not point to any context-specific objects!).
DirectHandle<Map> CreateStructMap(
    Isolate* isolate, wasm::CanonicalTypeIndex type,
    DirectHandle<Map> opt_rtt_parent, int num_supertypes,
    DirectHandle<NativeContext> opt_native_context);

DirectHandle<Map> CreateArrayMap(Isolate* isolate,
                                 wasm::CanonicalTypeIndex array_index,
                                 DirectHandle<Map> opt_rtt_parent,
                                 int num_supertypes);

DirectHandle<Map> CreateFuncRefMap(Isolate* isolate,
                                   wasm::CanonicalTypeIndex type,
                                   DirectHandle<Map> opt_rtt_parent,
                                   int num_supertypes, SharedFlag shared);

DirectHandle<Map> CreateContRefMap(Isolate* isolate,
                                   wasm::CanonicalTypeIndex type);

namespace wasm {
// Takes a {value} in the JS representation and typechecks it according to
// {expected}. If the typecheck succeeds, returns the wasm representation of the
// object; otherwise, returns the empty handle.
MaybeDirectHandle<Object> JSToWasmObject(Isolate* isolate,
                                         DirectHandle<Object> value,
                                         CanonicalValueType expected,
                                         const char** error_message);

// Utility which canonicalizes {expected} in addition.
MaybeDirectHandle<Object> JSToWasmObject(Isolate* isolate,
                                         const WasmModule* module,
                                         DirectHandle<Object> value,
                                         ValueType expected,
                                         const char** error_message);

// Takes a {value} in the Wasm representation and transforms it to the
// respective JS representation. The caller is responsible for not providing an
// object which cannot be transformed to JS.
DirectHandle<Object> WasmToJSObject(Isolate* isolate,
                                    DirectHandle<Object> value);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_WASM_WASM_OBJECTS_H_
