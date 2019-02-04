#include "node_messaging.h"
#include "async_wrap-inl.h"
#include "async_wrap.h"
#include "debug_utils.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "node_process.h"
#include "util-inl.h"
#include "util.h"

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferCreationMode;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::SharedArrayBuffer;
using v8::String;
using v8::Value;
using v8::ValueDeserializer;
using v8::ValueSerializer;
using v8::WasmCompiledModule;

namespace node {
namespace worker {

Message::Message(MallocedBuffer<char>&& buffer)
    : main_message_buf_(std::move(buffer)) {}

namespace {

// This is used to tell V8 how to read transferred host objects, like other
// `MessagePort`s and `SharedArrayBuffer`s, and make new JS objects out of them.
class DeserializerDelegate : public ValueDeserializer::Delegate {
 public:
  DeserializerDelegate(
      Message* m,
      Environment* env,
      const std::vector<MessagePort*>& message_ports,
      const std::vector<Local<SharedArrayBuffer>>& shared_array_buffers,
      const std::vector<WasmCompiledModule::TransferrableModule>& wasm_modules)
      : message_ports_(message_ports),
        shared_array_buffers_(shared_array_buffers),
        wasm_modules_(wasm_modules) {}

  MaybeLocal<Object> ReadHostObject(Isolate* isolate) override {
    // Currently, only MessagePort hosts objects are supported, so identifying
    // by the index in the message's MessagePort array is sufficient.
    uint32_t id;
    if (!deserializer->ReadUint32(&id))
      return MaybeLocal<Object>();
    CHECK_LE(id, message_ports_.size());
    return message_ports_[id]->object(isolate);
  };

  MaybeLocal<SharedArrayBuffer> GetSharedArrayBufferFromId(
      Isolate* isolate, uint32_t clone_id) override {
    CHECK_LE(clone_id, shared_array_buffers_.size());
    return shared_array_buffers_[clone_id];
  }

  MaybeLocal<WasmCompiledModule> GetWasmModuleFromId(
      Isolate* isolate, uint32_t transfer_id) override {
    CHECK_LE(transfer_id, wasm_modules_.size());
    return WasmCompiledModule::FromTransferrableModule(
        isolate, wasm_modules_[transfer_id]);
  }

  ValueDeserializer* deserializer = nullptr;

 private:
  const std::vector<MessagePort*>& message_ports_;
  const std::vector<Local<SharedArrayBuffer>>& shared_array_buffers_;
  const std::vector<WasmCompiledModule::TransferrableModule>& wasm_modules_;
};

}  // anonymous namespace

MaybeLocal<Value> Message::Deserialize(Environment* env,
                                       Local<Context> context) {
  EscapableHandleScope handle_scope(env->isolate());
  Context::Scope context_scope(context);

  // Create all necessary MessagePort handles.
  std::vector<MessagePort*> ports(message_ports_.size());
  for (uint32_t i = 0; i < message_ports_.size(); ++i) {
    ports[i] = MessagePort::New(env,
                                context,
                                std::move(message_ports_[i]));
    if (ports[i] == nullptr) {
      for (MessagePort* port : ports) {
        // This will eventually release the MessagePort object itself.
        if (port != nullptr)
          port->Close();
      }
      return MaybeLocal<Value>();
    }
  }
  message_ports_.clear();

  std::vector<Local<SharedArrayBuffer>> shared_array_buffers;
  // Attach all transferred SharedArrayBuffers to their new Isolate.
  for (uint32_t i = 0; i < shared_array_buffers_.size(); ++i) {
    Local<SharedArrayBuffer> sab;
    if (!shared_array_buffers_[i]->GetSharedArrayBuffer(env, context)
            .ToLocal(&sab))
      return MaybeLocal<Value>();
    shared_array_buffers.push_back(sab);
  }
  shared_array_buffers_.clear();

  DeserializerDelegate delegate(
      this, env, ports, shared_array_buffers, wasm_modules_);
  ValueDeserializer deserializer(
      env->isolate(),
      reinterpret_cast<const uint8_t*>(main_message_buf_.data),
      main_message_buf_.size,
      &delegate);
  delegate.deserializer = &deserializer;

  // Attach all transferred ArrayBuffers to their new Isolate.
  for (uint32_t i = 0; i < array_buffer_contents_.size(); ++i) {
    Local<ArrayBuffer> ab =
        ArrayBuffer::New(env->isolate(),
                         array_buffer_contents_[i].release(),
                         array_buffer_contents_[i].size,
                         ArrayBufferCreationMode::kInternalized);
    deserializer.TransferArrayBuffer(i, ab);
  }
  array_buffer_contents_.clear();

  if (deserializer.ReadHeader(context).IsNothing())
    return MaybeLocal<Value>();
  return handle_scope.Escape(
      deserializer.ReadValue(context).FromMaybe(Local<Value>()));
}

void Message::AddSharedArrayBuffer(
    SharedArrayBufferMetadataReference reference) {
  shared_array_buffers_.push_back(reference);
}

void Message::AddMessagePort(std::unique_ptr<MessagePortData>&& data) {
  message_ports_.emplace_back(std::move(data));
}

uint32_t Message::AddWASMModule(WasmCompiledModule::TransferrableModule&& mod) {
  wasm_modules_.emplace_back(std::move(mod));
  return wasm_modules_.size() - 1;
}

namespace {

void ThrowDataCloneException(Environment* env, Local<String> message) {
  Local<Value> argv[] = {
    message,
    FIXED_ONE_BYTE_STRING(env->isolate(), "DataCloneError")
  };
  Local<Value> exception;
  Local<Function> domexception_ctor = env->domexception_function();
  CHECK(!domexception_ctor.IsEmpty());
  if (!domexception_ctor->NewInstance(env->context(), arraysize(argv), argv)
          .ToLocal(&exception)) {
    return;
  }
  env->isolate()->ThrowException(exception);
}

// This tells V8 how to serialize objects that it does not understand
// (e.g. C++ objects) into the output buffer, in a way that our own
// DeserializerDelegate understands how to unpack.
class SerializerDelegate : public ValueSerializer::Delegate {
 public:
  SerializerDelegate(Environment* env, Local<Context> context, Message* m)
      : env_(env), context_(context), msg_(m) {}

  void ThrowDataCloneError(Local<String> message) override {
    ThrowDataCloneException(env_, message);
  }

  Maybe<bool> WriteHostObject(Isolate* isolate, Local<Object> object) override {
    if (env_->message_port_constructor_template()->HasInstance(object)) {
      return WriteMessagePort(Unwrap<MessagePort>(object));
    }

    THROW_ERR_CANNOT_TRANSFER_OBJECT(env_);
    return Nothing<bool>();
  }

  Maybe<uint32_t> GetSharedArrayBufferId(
      Isolate* isolate,
      Local<SharedArrayBuffer> shared_array_buffer) override {
    uint32_t i;
    for (i = 0; i < seen_shared_array_buffers_.size(); ++i) {
      if (seen_shared_array_buffers_[i] == shared_array_buffer)
        return Just(i);
    }

    auto reference = SharedArrayBufferMetadata::ForSharedArrayBuffer(
        env_,
        context_,
        shared_array_buffer);
    if (!reference) {
      return Nothing<uint32_t>();
    }
    seen_shared_array_buffers_.push_back(shared_array_buffer);
    msg_->AddSharedArrayBuffer(reference);
    return Just(i);
  }

  Maybe<uint32_t> GetWasmModuleTransferId(
      Isolate* isolate, Local<WasmCompiledModule> module) override {
    return Just(msg_->AddWASMModule(module->GetTransferrableModule()));
  }

  void Finish() {
    // Only close the MessagePort handles and actually transfer them
    // once we know that serialization succeeded.
    for (MessagePort* port : ports_) {
      port->Close();
      msg_->AddMessagePort(port->Detach());
    }
  }

  ValueSerializer* serializer = nullptr;

 private:
  Maybe<bool> WriteMessagePort(MessagePort* port) {
    for (uint32_t i = 0; i < ports_.size(); i++) {
      if (ports_[i] == port) {
        serializer->WriteUint32(i);
        return Just(true);
      }
    }

    THROW_ERR_MISSING_MESSAGE_PORT_IN_TRANSFER_LIST(env_);
    return Nothing<bool>();
  }

  Environment* env_;
  Local<Context> context_;
  Message* msg_;
  std::vector<Local<SharedArrayBuffer>> seen_shared_array_buffers_;
  std::vector<MessagePort*> ports_;

  friend class worker::Message;
};

}  // anonymous namespace

Maybe<bool> Message::Serialize(Environment* env,
                               Local<Context> context,
                               Local<Value> input,
                               Local<Value> transfer_list_v,
                               Local<Object> source_port) {
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(context);

  // Verify that we're not silently overwriting an existing message.
  CHECK(main_message_buf_.is_empty());

  SerializerDelegate delegate(env, context, this);
  ValueSerializer serializer(env->isolate(), &delegate);
  delegate.serializer = &serializer;

  std::vector<Local<ArrayBuffer>> array_buffers;
  if (transfer_list_v->IsArray()) {
    Local<Array> transfer_list = transfer_list_v.As<Array>();
    uint32_t length = transfer_list->Length();
    for (uint32_t i = 0; i < length; ++i) {
      Local<Value> entry;
      if (!transfer_list->Get(context, i).ToLocal(&entry))
        return Nothing<bool>();
      // Currently, we support ArrayBuffers and MessagePorts.
      if (entry->IsArrayBuffer()) {
        Local<ArrayBuffer> ab = entry.As<ArrayBuffer>();
        // If we cannot render the ArrayBuffer unusable in this Isolate and
        // take ownership of its memory, copying the buffer will have to do.
        if (!ab->IsNeuterable() || ab->IsExternal())
          continue;
        if (std::find(array_buffers.begin(), array_buffers.end(), ab) !=
            array_buffers.end()) {
          ThrowDataCloneException(
              env,
              FIXED_ONE_BYTE_STRING(
                  env->isolate(),
                  "Transfer list contains duplicate ArrayBuffer"));
          return Nothing<bool>();
        }
        // We simply use the array index in the `array_buffers` list as the
        // ID that we write into the serialized buffer.
        uint32_t id = array_buffers.size();
        array_buffers.push_back(ab);
        serializer.TransferArrayBuffer(id, ab);
        continue;
      } else if (env->message_port_constructor_template()
                    ->HasInstance(entry)) {
        // Check if the source MessagePort is being transferred.
        if (!source_port.IsEmpty() && entry == source_port) {
          ThrowDataCloneException(
              env,
              FIXED_ONE_BYTE_STRING(env->isolate(),
                                    "Transfer list contains source port"));
          return Nothing<bool>();
        }
        MessagePort* port = Unwrap<MessagePort>(entry.As<Object>());
        if (port == nullptr || port->IsDetached()) {
          ThrowDataCloneException(
              env,
              FIXED_ONE_BYTE_STRING(
                  env->isolate(),
                  "MessagePort in transfer list is already detached"));
          return Nothing<bool>();
        }
        if (std::find(delegate.ports_.begin(), delegate.ports_.end(), port) !=
            delegate.ports_.end()) {
          ThrowDataCloneException(
              env,
              FIXED_ONE_BYTE_STRING(
                  env->isolate(),
                  "Transfer list contains duplicate MessagePort"));
          return Nothing<bool>();
        }
        delegate.ports_.push_back(port);
        continue;
      }

      THROW_ERR_INVALID_TRANSFER_OBJECT(env);
      return Nothing<bool>();
    }
  }

  serializer.WriteHeader();
  if (serializer.WriteValue(context, input).IsNothing()) {
    return Nothing<bool>();
  }

  for (Local<ArrayBuffer> ab : array_buffers) {
    // If serialization succeeded, we want to take ownership of
    // (a.k.a. externalize) the underlying memory region and render
    // it inaccessible in this Isolate.
    ArrayBuffer::Contents contents = ab->Externalize();
    ab->Neuter();
    array_buffer_contents_.push_back(
        MallocedBuffer<char> { static_cast<char*>(contents.Data()),
                               contents.ByteLength() });
  }

  delegate.Finish();

  // The serializer gave us a buffer allocated using `malloc()`.
  std::pair<uint8_t*, size_t> data = serializer.Release();
  main_message_buf_ =
      MallocedBuffer<char>(reinterpret_cast<char*>(data.first), data.second);
  return Just(true);
}

void Message::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("array_buffer_contents", array_buffer_contents_);
  tracker->TrackFieldWithSize("shared_array_buffers",
      shared_array_buffers_.size() * sizeof(shared_array_buffers_[0]));
  tracker->TrackField("message_ports", message_ports_);
}

MessagePortData::MessagePortData(MessagePort* owner) : owner_(owner) { }

MessagePortData::~MessagePortData() {
  CHECK_NULL(owner_);
  Disentangle();
}

void MessagePortData::MemoryInfo(MemoryTracker* tracker) const {
  Mutex::ScopedLock lock(mutex_);
  tracker->TrackField("incoming_messages", incoming_messages_);
}

void MessagePortData::AddToIncomingQueue(Message&& message) {
  // This function will be called by other threads.
  Mutex::ScopedLock lock(mutex_);
  incoming_messages_.emplace_back(std::move(message));

  if (owner_ != nullptr) {
    Debug(owner_, "Adding message to incoming queue");
    owner_->TriggerAsync();
  }
}

bool MessagePortData::IsSiblingClosed() const {
  Mutex::ScopedLock lock(*sibling_mutex_);
  return sibling_ == nullptr;
}

void MessagePortData::Entangle(MessagePortData* a, MessagePortData* b) {
  CHECK_NULL(a->sibling_);
  CHECK_NULL(b->sibling_);
  a->sibling_ = b;
  b->sibling_ = a;
  a->sibling_mutex_ = b->sibling_mutex_;
}

void MessagePortData::PingOwnerAfterDisentanglement() {
  Mutex::ScopedLock lock(mutex_);
  if (owner_ != nullptr)
    owner_->TriggerAsync();
}

void MessagePortData::Disentangle() {
  // Grab a copy of the sibling mutex, then replace it so that each sibling
  // has its own sibling_mutex_ now.
  std::shared_ptr<Mutex> sibling_mutex = sibling_mutex_;
  Mutex::ScopedLock sibling_lock(*sibling_mutex);
  sibling_mutex_ = std::make_shared<Mutex>();

  MessagePortData* sibling = sibling_;
  if (sibling_ != nullptr) {
    sibling_->sibling_ = nullptr;
    sibling_ = nullptr;
  }

  // We close MessagePorts after disentanglement, so we trigger the
  // corresponding uv_async_t to let them know that this happened.
  PingOwnerAfterDisentanglement();
  if (sibling != nullptr) {
    sibling->PingOwnerAfterDisentanglement();
  }
}

MessagePort::~MessagePort() {
  if (data_)
    data_->owner_ = nullptr;
}

MessagePort::MessagePort(Environment* env,
                         Local<Context> context,
                         Local<Object> wrap)
  : HandleWrap(env,
               wrap,
               reinterpret_cast<uv_handle_t*>(new uv_async_t()),
               AsyncWrap::PROVIDER_MESSAGEPORT),
    data_(new MessagePortData(this)) {
  auto onmessage = [](uv_async_t* handle) {
    // Called when data has been put into the queue.
    MessagePort* channel = static_cast<MessagePort*>(handle->data);
    channel->OnMessage();
  };
  CHECK_EQ(uv_async_init(env->event_loop(),
                         async(),
                         onmessage), 0);
  async()->data = static_cast<void*>(this);

  Local<Value> fn;
  if (!wrap->Get(context, env->oninit_symbol()).ToLocal(&fn))
    return;

  if (fn->IsFunction()) {
    Local<Function> init = fn.As<Function>();
    USE(init->Call(context, wrap, 0, nullptr));
  }

  Debug(this, "Created message port");
}

void MessagePort::AddToIncomingQueue(Message&& message) {
  data_->AddToIncomingQueue(std::move(message));
}

uv_async_t* MessagePort::async() {
  return reinterpret_cast<uv_async_t*>(GetHandle());
}

bool MessagePort::IsDetached() const {
  return data_ == nullptr || IsHandleClosing();
}

void MessagePort::TriggerAsync() {
  if (IsHandleClosing()) return;
  CHECK_EQ(uv_async_send(async()), 0);
}

void MessagePort::Close(v8::Local<v8::Value> close_callback) {
  Debug(this, "Closing message port, data set = %d", static_cast<int>(!!data_));

  if (data_) {
    // Wrap this call with accessing the mutex, so that TriggerAsync()
    // can check IsHandleClosing() without race conditions.
    Mutex::ScopedLock sibling_lock(data_->mutex_);
    HandleWrap::Close(close_callback);
  } else {
    HandleWrap::Close(close_callback);
  }
}

void MessagePort::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!args.IsConstructCall()) {
    THROW_ERR_CONSTRUCT_CALL_REQUIRED(env);
    return;
  }

  Local<Context> context = args.This()->CreationContext();
  Context::Scope context_scope(context);

  new MessagePort(env, context, args.This());
}

MessagePort* MessagePort::New(
    Environment* env,
    Local<Context> context,
    std::unique_ptr<MessagePortData> data) {
  Context::Scope context_scope(context);
  Local<Function> ctor;
  if (!GetMessagePortConstructor(env, context).ToLocal(&ctor))
    return nullptr;

  // Construct a new instance, then assign the listener instance and possibly
  // the MessagePortData to it.
  Local<Object> instance;
  if (!ctor->NewInstance(context).ToLocal(&instance))
    return nullptr;
  MessagePort* port = Unwrap<MessagePort>(instance);
  CHECK_NOT_NULL(port);
  if (data) {
    port->Detach();
    port->data_ = std::move(data);

    // This lock is here to avoid race conditions with the `owner_` read
    // in AddToIncomingQueue(). (This would likely be unproblematic without it,
    // but it's better to be safe than sorry.)
    Mutex::ScopedLock lock(port->data_->mutex_);
    port->data_->owner_ = port;
    // If the existing MessagePortData object had pending messages, this is
    // the easiest way to run that queue.
    port->TriggerAsync();
  }
  return port;
}

void MessagePort::OnMessage() {
  Debug(this, "Running MessagePort::OnMessage()");
  HandleScope handle_scope(env()->isolate());
  Local<Context> context = object(env()->isolate())->CreationContext();

  // data_ can only ever be modified by the owner thread, so no need to lock.
  // However, the message port may be transferred while it is processing
  // messages, so we need to check that this handle still owns its `data_` field
  // on every iteration.
  while (data_) {
    Message received;
    {
      // Get the head of the message queue.
      Mutex::ScopedLock lock(data_->mutex_);

      if (stop_event_loop_) {
        Debug(this, "MessagePort stops loop as requested");
        CHECK(!data_->receiving_messages_);
        uv_stop(env()->event_loop());
        break;
      }

      Debug(this, "MessagePort has message, receiving = %d",
            static_cast<int>(data_->receiving_messages_));

      if (!data_->receiving_messages_)
        break;
      if (data_->incoming_messages_.empty())
        break;
      received = std::move(data_->incoming_messages_.front());
      data_->incoming_messages_.pop_front();
    }

    if (!env()->can_call_into_js()) {
      Debug(this, "MessagePort drains queue because !can_call_into_js()");
      // In this case there is nothing to do but to drain the current queue.
      continue;
    }

    {
      // Call the JS .onmessage() callback.
      HandleScope handle_scope(env()->isolate());
      Context::Scope context_scope(context);
      Local<Value> args[] = {
        received.Deserialize(env(), context).FromMaybe(Local<Value>())
      };

      if (args[0].IsEmpty() ||
          MakeCallback(env()->onmessage_string(), 1, args).IsEmpty()) {
        // Re-schedule OnMessage() execution in case of failure.
        if (data_)
          TriggerAsync();
        return;
      }
    }
  }

  if (data_ && data_->IsSiblingClosed()) {
    Close();
  }
}

bool MessagePort::IsSiblingClosed() const {
  CHECK(data_);
  return data_->IsSiblingClosed();
}

void MessagePort::OnClose() {
  Debug(this, "MessagePort::OnClose()");
  if (data_) {
    data_->owner_ = nullptr;
    data_->Disentangle();
  }
  data_.reset();
  delete async();
}

std::unique_ptr<MessagePortData> MessagePort::Detach() {
  CHECK(data_);
  Mutex::ScopedLock lock(data_->mutex_);
  data_->owner_ = nullptr;
  return std::move(data_);
}


Maybe<bool> MessagePort::PostMessage(Environment* env,
                                     Local<Value> message_v,
                                     Local<Value> transfer_v) {
  Isolate* isolate = env->isolate();
  Local<Object> obj = object(isolate);
  Local<Context> context = obj->CreationContext();

  Message msg;

  // Per spec, we need to both check if transfer list has the source port, and
  // serialize the input message, even if the MessagePort is closed or detached.

  Maybe<bool> serialization_maybe =
      msg.Serialize(env, context, message_v, transfer_v, obj);
  if (data_ == nullptr) {
    return serialization_maybe;
  }
  if (serialization_maybe.IsNothing()) {
    return Nothing<bool>();
  }

  Mutex::ScopedLock lock(*data_->sibling_mutex_);
  bool doomed = false;

  // Check if the target port is posted to itself.
  if (data_->sibling_ != nullptr) {
    for (const auto& port_data : msg.message_ports()) {
      if (data_->sibling_ == port_data.get()) {
        doomed = true;
        ProcessEmitWarning(env, "The target port was posted to itself, and "
                                "the communication channel was lost");
        break;
      }
    }
  }

  if (data_->sibling_ == nullptr || doomed)
    return Just(true);

  data_->sibling_->AddToIncomingQueue(std::move(msg));
  return Just(true);
}

void MessagePort::PostMessage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() == 0) {
    return THROW_ERR_MISSING_ARGS(env, "Not enough arguments to "
                                       "MessagePort.postMessage");
  }

  MessagePort* port = Unwrap<MessagePort>(args.This());
  // Even if the backing MessagePort object has already been deleted, we still
  // want to serialize the message to ensure spec-compliant behavior w.r.t.
  // transfers.
  if (port == nullptr) {
    Message msg;
    Local<Object> obj = args.This();
    Local<Context> context = obj->CreationContext();
    USE(msg.Serialize(env, context, args[0], args[1], obj));
    return;
  }

  port->PostMessage(env, args[0], args[1]);
}

void MessagePort::Start() {
  Mutex::ScopedLock lock(data_->mutex_);
  Debug(this, "Start receiving messages");
  data_->receiving_messages_ = true;
  if (!data_->incoming_messages_.empty())
    TriggerAsync();
}

void MessagePort::Stop() {
  Mutex::ScopedLock lock(data_->mutex_);
  Debug(this, "Stop receiving messages");
  data_->receiving_messages_ = false;
}

void MessagePort::StopEventLoop() {
  Mutex::ScopedLock lock(data_->mutex_);
  data_->receiving_messages_ = false;
  stop_event_loop_ = true;

  Debug(this, "Received StopEventLoop request");
  TriggerAsync();
}

void MessagePort::Start(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  MessagePort* port;
  ASSIGN_OR_RETURN_UNWRAP(&port, args.This());
  if (!port->data_) {
    THROW_ERR_CLOSED_MESSAGE_PORT(env);
    return;
  }
  port->Start();
}

void MessagePort::Stop(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  MessagePort* port;
  ASSIGN_OR_RETURN_UNWRAP(&port, args.This());
  if (!port->data_) {
    THROW_ERR_CLOSED_MESSAGE_PORT(env);
    return;
  }
  port->Stop();
}

void MessagePort::Drain(const FunctionCallbackInfo<Value>& args) {
  MessagePort* port;
  ASSIGN_OR_RETURN_UNWRAP(&port, args.This());
  port->OnMessage();
}

void MessagePort::Entangle(MessagePort* a, MessagePort* b) {
  Entangle(a, b->data_.get());
}

void MessagePort::Entangle(MessagePort* a, MessagePortData* b) {
  MessagePortData::Entangle(a->data_.get(), b);
}

MaybeLocal<Function> GetMessagePortConstructor(
    Environment* env, Local<Context> context) {
  // Factor generating the MessagePort JS constructor into its own piece
  // of code, because it is needed early on in the child environment setup.
  Local<FunctionTemplate> templ = env->message_port_constructor_template();
  if (!templ.IsEmpty())
    return templ->GetFunction(context);

  {
    Local<FunctionTemplate> m = env->NewFunctionTemplate(MessagePort::New);
    m->SetClassName(env->message_port_constructor_string());
    m->InstanceTemplate()->SetInternalFieldCount(1);
    m->Inherit(HandleWrap::GetConstructorTemplate(env));

    env->SetProtoMethod(m, "postMessage", MessagePort::PostMessage);
    env->SetProtoMethod(m, "start", MessagePort::Start);
    env->SetProtoMethod(m, "stop", MessagePort::Stop);
    env->SetProtoMethod(m, "drain", MessagePort::Drain);

    env->set_message_port_constructor_template(m);
  }

  return GetMessagePortConstructor(env, context);
}

namespace {

static void MessageChannel(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!args.IsConstructCall()) {
    THROW_ERR_CONSTRUCT_CALL_REQUIRED(env);
    return;
  }

  Local<Context> context = args.This()->CreationContext();
  Context::Scope context_scope(context);

  MessagePort* port1 = MessagePort::New(env, context);
  MessagePort* port2 = MessagePort::New(env, context);
  MessagePort::Entangle(port1, port2);

  args.This()->Set(env->context(), env->port1_string(), port1->object())
      .FromJust();
  args.This()->Set(env->context(), env->port2_string(), port2->object())
      .FromJust();
}

static void RegisterDOMException(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  env->set_domexception_function(args[0].As<Function>());
}

static void InitMessaging(Local<Object> target,
                          Local<Value> unused,
                          Local<Context> context,
                          void* priv) {
  Environment* env = Environment::GetCurrent(context);

  {
    Local<String> message_channel_string =
        FIXED_ONE_BYTE_STRING(env->isolate(), "MessageChannel");
    Local<FunctionTemplate> templ = env->NewFunctionTemplate(MessageChannel);
    templ->SetClassName(message_channel_string);
    target->Set(env->context(),
                message_channel_string,
                templ->GetFunction(context).ToLocalChecked()).FromJust();
  }

  target->Set(context,
              env->message_port_constructor_string(),
              GetMessagePortConstructor(env, context).ToLocalChecked())
                  .FromJust();

  env->SetMethod(target, "registerDOMException", RegisterDOMException);
}

}  // anonymous namespace

}  // namespace worker
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(messaging, node::worker::InitMessaging)
