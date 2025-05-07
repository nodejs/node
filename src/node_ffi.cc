#if HAVE_FFI

#include "node_ffi.h"
#include "env-inl.h"
#include "node_buffer.h"

namespace node::ffi {
using binding::DLib;
using permission::PermissionScope;
using v8::Array;
using v8::ArrayBuffer;
using v8::BackingStore;
using v8::BigInt;
using v8::Boolean;
using v8::Context;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::Number;
using v8::Object;
using v8::Persistent;
using v8::Uint32;
using v8::Value;

void* readAddress(const Local<Value> value) {
  if (value->IsExternal()) {
    return value.As<External>()->Value();
  }
  if (value->IsArrayBuffer()) {
    return value.As<ArrayBuffer>()->Data();
  }
  if (value->IsArrayBufferView()) {
    return Buffer::Data(value);
  }
  if (value->IsBigInt()) {
    return reinterpret_cast<void*>(value.As<BigInt>()->Uint64Value());
  }
  return nullptr;
}

int64_t readInt64(const Local<Value> value) {
  if (value->IsInt32()) {
    return value.As<Int32>()->Value();
  }
  if (value->IsUint32()) {
    return value.As<Uint32>()->Value();
  }
  if (value->IsNumber()) {
    return static_cast<int64_t>(value.As<Number>()->Value());
  }
  if (value->IsBigInt()) {
    return value.As<BigInt>()->Int64Value();
  }
  return 0;
}

uint64_t readUInt64(const Local<Value> value) {
  if (value->IsUint32()) {
    return value.As<Uint32>()->Value();
  }
  if (value->IsInt32()) {
    return value.As<Int32>()->Value();
  }
  if (value->IsNumber()) {
    return static_cast<uint64_t>(value.As<Number>()->Value());
  }
  if (value->IsBigInt()) {
    return value.As<BigInt>()->Uint64Value();
  }
  return 0;
}

double readDouble(const Local<Value> value) {
  if (value->IsNumber()) {
    return value.As<Number>()->Value();
  }
  if (value->IsBigInt()) {
    return static_cast<double>(value.As<BigInt>()->Int64Value());
  }
  return 0.0;
}

std::string readString(Isolate* isolate, const Local<Value> value) {
  if (value->IsString()) {
    return Utf8Value(isolate, value).ToString();
  }
  return "";
}

void ffiCallback(ffi_cif* cif, void* ret, ffi_raw* args, void* hint) {
  const auto self = static_cast<FFICallback*>(hint);
  self->doCallback(
      static_cast<ffi_raw*>(ret), static_cast<int>(cif->nargs), args);
}

FFILibrary::FFILibrary(const char* path) : DLib(path, kDefaultFlags) {}

FFILibrary::~FFILibrary() {
  Close();
}

FFIDefinition::FFIDefinition(const char* defStr) {
  const auto length = strlen(defStr);
  if (length < 1) {
    UNREACHABLE("Bad defStr size");
  }
  types = std::make_unique<ffi_type*[]>(length);
  for (int i = 0; i < length; i++) {
    // These field definitions refer to dyncall
    switch (defStr[i]) {
      case 'v':
        types[i] = &ffi_type_void;
        break;
      case 'C':
        types[i] = &ffi_type_uint8;
        break;
      case 'c':
        types[i] = &ffi_type_sint8;
        break;
      case 'S':
        types[i] = &ffi_type_uint16;
        break;
      case 's':
        types[i] = &ffi_type_sint16;
        break;
      case 'I':
        types[i] = &ffi_type_uint32;
        break;
      case 'i':
        types[i] = &ffi_type_sint32;
        break;
      case 'L':
        types[i] = &ffi_type_uint64;
        break;
      case 'l':
        types[i] = &ffi_type_sint64;
        break;
      case 'f':
        types[i] = &ffi_type_float;
        break;
      case 'p':
        types[i] = &ffi_type_pointer;
        break;
      default:
        UNREACHABLE("Bad FFI type");
    }
  }
  if (ffi_prep_cif(&cif, ABI, length - 1, types[0], &types[1]) != FFI_OK) {
    UNREACHABLE("ffi_prep_cif Failed");
  }
}

void FFIDefinition::readValue(const int i,
                              const Local<Value> input,
                              ffi_raw* output) const {
  switch (types[i]->type) {
    case FFI_TYPE_VOID:
      output->uint = 0;
      break;
    case FFI_TYPE_UINT8:
    case FFI_TYPE_UINT16:
    case FFI_TYPE_UINT32:
    case FFI_TYPE_UINT64:
      output->uint = readUInt64(input);
      break;
    case FFI_TYPE_SINT8:
    case FFI_TYPE_SINT16:
    case FFI_TYPE_SINT32:
    case FFI_TYPE_SINT64:
      output->sint = readInt64(input);
      break;
    case FFI_TYPE_FLOAT:
      output->flt = static_cast<float>(readDouble(input));
      break;
    case FFI_TYPE_POINTER:
      output->ptr = readAddress(input);
      break;
    default:
      UNREACHABLE("Bad FFI type");
  }
}

Local<Value> FFIDefinition::wrapValue(const int i,
                                      Isolate* isolate,
                                      const ffi_raw* input) const {
  switch (types[i]->type) {
    case FFI_TYPE_VOID:
      return Undefined(isolate);
    case FFI_TYPE_UINT8:
    case FFI_TYPE_UINT16:
    case FFI_TYPE_UINT32:
      return Uint32::NewFromUnsigned(isolate,
                                     static_cast<uint32_t>(input->uint));
    case FFI_TYPE_UINT64:
      return BigInt::NewFromUnsigned(isolate, input->uint);
    case FFI_TYPE_SINT8:
    case FFI_TYPE_SINT16:
    case FFI_TYPE_SINT32:
      return Int32::New(isolate, static_cast<int32_t>(input->sint));
    case FFI_TYPE_SINT64:
      return BigInt::New(isolate, input->sint);
    case FFI_TYPE_FLOAT:
      return Number::New(isolate, input->flt);
    case FFI_TYPE_POINTER:
      return External::New(isolate, input->ptr);
    default:
      UNREACHABLE("Bad FFI type");
  }
}

FFIInvoker::FFIInvoker(const char* defStr, const void* address)
    : FFIDefinition(defStr) {
  invoker = FFI_FN(address);
  datas = std::make_unique<ffi_raw[]>(cif.nargs);
}

void FFIInvoker::setParam(const int i, const Local<Value> value) {
  readValue(i, value, &datas[i - 1]);
}

Local<Value> FFIInvoker::doInvoke(Isolate* isolate) {
  ffi_raw result;
  ffi_raw_call(&cif, invoker, &result, datas.get());
  return wrapValue(0, isolate, &result);
}

FFICallback::FFICallback(const char* defStr) : FFIDefinition(defStr) {
  frc = static_cast<ffi_raw_closure*>(ffi_closure_alloc(RCS, &address));
  if (!frc) {
    UNREACHABLE("ffi_closure_alloc Failed");
  }
  if (ffi_prep_raw_closure(frc, &cif, ffiCallback, this) != FFI_OK) {
    UNREACHABLE("ffi_prep_raw_closure Failed");
  }
}

void FFICallback::setCallback(Isolate* isolate, const Local<Value> value) {
  if (value->IsFunction()) {
    const auto function = value.As<Function>();
    callback.Reset(isolate, function);
  }
}

void FFICallback::doCallback(ffi_raw* result,
                             const int argc,
                             const ffi_raw* args) const {
  const auto isolate = Isolate::GetCurrent();
  LocalVector<Value> params(isolate);
  params.reserve(argc);
  for (int i = 0; i < argc; i++) {
    params.emplace_back(wrapValue(i + 1, isolate, args + i));
  }
  const auto function = callback.Get(isolate);
  const auto context = function->GetCreationContextChecked(isolate);
  const auto global = Undefined(isolate);
  const auto return1 =
      function->Call(isolate, context, global, argc, params.data());
  Local<Value> return2;
  if (return1.ToLocal(&return2)) {
    readValue(0, return2, result);
  }
}

FFICallback::~FFICallback() {
  ffi_closure_free(frc);
  callback.Reset();
}

void CallInvoker(const FunctionCallbackInfo<Value>& args) {
  const auto isolate = args.GetIsolate();
  const auto length = args.Length();
  if (const auto invoker = static_cast<FFIInvoker*>(readAddress(args[0]))) {
    for (int i = 1; i < length; i++) {
      invoker->setParam(i, args[i]);
    }
    args.GetReturnValue().Set(invoker->doInvoke(isolate));
  }
}

void CreateBuffer(const FunctionCallbackInfo<Value>& args) {
  const auto isolate = args.GetIsolate();
  const auto env = Environment::GetCurrent(isolate);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, PermissionScope::kFFI, "");
  if (const auto address = readAddress(args[0])) {
    const auto length = readUInt64(args[1]);
    auto store = ArrayBuffer::NewBackingStore(
        address, length, BackingStore::EmptyDeleter, nullptr);
    args.GetReturnValue().Set(ArrayBuffer::New(isolate, std::move(store)));
  }
}

void CreateCallback(const FunctionCallbackInfo<Value>& args) {
  const auto isolate = args.GetIsolate();
  const auto env = Environment::GetCurrent(isolate);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, PermissionScope::kFFI, "");
  const auto defStr = readString(isolate, args[0]);
  const auto callback = new FFICallback(defStr.c_str());
  callback->setCallback(isolate, args[1]);
  Local<Value> result[] = {External::New(isolate, callback),
                           External::New(isolate, callback->address)};
  args.GetReturnValue().Set(Array::New(isolate, result, 2));
}

void CreateInvoker(const FunctionCallbackInfo<Value>& args) {
  const auto isolate = args.GetIsolate();
  const auto env = Environment::GetCurrent(isolate);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, PermissionScope::kFFI, "");
  if (const auto address = readAddress(args[0])) {
    const auto defStr = readString(isolate, args[1]);
    const auto invoker = new FFIInvoker(defStr.c_str(), address);
    args.GetReturnValue().Set(External::New(isolate, invoker));
  }
}

void FindSymbol(const FunctionCallbackInfo<Value>& args) {
  const auto isolate = args.GetIsolate();
  if (const auto library = static_cast<FFILibrary*>(readAddress(args[0]))) {
    const auto symbol = readString(isolate, args[1]);
    if (const auto address = library->GetSymbolAddress(symbol.c_str())) {
      args.GetReturnValue().Set(External::New(isolate, address));
    }
  }
}

void FreeCallback(const FunctionCallbackInfo<Value>& args) {
  if (const auto temp = static_cast<FFICallback*>(readAddress(args[0]))) {
    delete temp;
  }
}

void FreeInvoker(const FunctionCallbackInfo<Value>& args) {
  if (const auto temp = static_cast<FFIInvoker*>(readAddress(args[0]))) {
    delete temp;
  }
}

void FreeLibrary(const FunctionCallbackInfo<Value>& args) {
  if (const auto temp = static_cast<FFILibrary*>(readAddress(args[0]))) {
    delete temp;
  }
}

void GetAddress(const FunctionCallbackInfo<Value>& args) {
  const auto isolate = args.GetIsolate();
  const auto address = readAddress(args[0]);
  args.GetReturnValue().Set(
      BigInt::NewFromUnsigned(isolate, reinterpret_cast<uint64_t>(address)));
}

void LoadLibrary(const FunctionCallbackInfo<Value>& args) {
  const auto isolate = args.GetIsolate();
  const auto env = Environment::GetCurrent(isolate);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, PermissionScope::kFFI, "");
  const auto path = readString(isolate, args[0]);
  const auto library = new FFILibrary(path.c_str());
  if (library->Open()) {
    args.GetReturnValue().Set(External::New(isolate, library));
  } else {
    delete library;
  }
}

void SysIs64(const FunctionCallbackInfo<Value>& args) {
  const auto isolate = args.GetIsolate();
  args.GetReturnValue().Set(Boolean::New(isolate, sizeof(void*) == 8));
}

void SysIsLE(const FunctionCallbackInfo<Value>& args) {
  const auto isolate = args.GetIsolate();
  args.GetReturnValue().Set(Boolean::New(isolate, IsLittleEndian()));
}

void Initialize(const Local<Object> obj,
                const Local<Value>,
                const Local<Context> ctx,
                void*) {
  SetMethod(ctx, obj, "CallInvoker", CallInvoker);
  SetMethod(ctx, obj, "CreateBuffer", CreateBuffer);
  SetMethod(ctx, obj, "CreateCallback", CreateCallback);
  SetMethod(ctx, obj, "CreateInvoker", CreateInvoker);
  SetMethod(ctx, obj, "FindSymbol", FindSymbol);
  SetMethod(ctx, obj, "FreeCallback", FreeCallback);
  SetMethod(ctx, obj, "FreeInvoker", FreeInvoker);
  SetMethod(ctx, obj, "FreeLibrary", FreeLibrary);
  SetMethod(ctx, obj, "GetAddress", GetAddress);
  SetMethod(ctx, obj, "LoadLibrary", LoadLibrary);
  SetMethod(ctx, obj, "SysIs64", SysIs64);
  SetMethod(ctx, obj, "SysIsLE", SysIsLE);
}

void Register(ExternalReferenceRegistry* registry) {
  registry->Register(CallInvoker);
  registry->Register(CreateBuffer);
  registry->Register(CreateCallback);
  registry->Register(CreateInvoker);
  registry->Register(FindSymbol);
  registry->Register(FreeCallback);
  registry->Register(FreeInvoker);
  registry->Register(FreeLibrary);
  registry->Register(GetAddress);
  registry->Register(LoadLibrary);
  registry->Register(SysIs64);
  registry->Register(SysIsLE);
}
}  // namespace node::ffi

NODE_BINDING_CONTEXT_AWARE_INTERNAL(ffi, node::ffi::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(ffi, node::ffi::Register)

#endif  // HAVE_FFI
