#ifndef SRC_NODE_MESSAGING_H_
#define SRC_NODE_MESSAGING_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "node_mutex.h"
#include <list>

namespace node {
namespace worker {

class MessagePortData;
class MessagePort;

typedef MaybeStackBuffer<v8::Local<v8::Value>, 8> TransferList;

// Represents a single communication message.
class Message : public MemoryRetainer {
 public:
  // Create a Message with a specific underlying payload, in the format of the
  // V8 ValueSerializer API. If `payload` is empty, this message indicates
  // that the receiving message port should close itself.
  explicit Message(MallocedBuffer<char>&& payload = MallocedBuffer<char>());

  Message(Message&& other) = default;
  Message& operator=(Message&& other) = default;
  Message& operator=(const Message&) = delete;
  Message(const Message&) = delete;

  // Whether this is a message indicating that the port is to be closed.
  // This is the last message to be received by a MessagePort.
  bool IsCloseMessage() const;

  // Deserialize the contained JS value. May only be called once, and only
  // after Serialize() has been called (e.g. by another thread).
  v8::MaybeLocal<v8::Value> Deserialize(Environment* env,
                                        v8::Local<v8::Context> context);

  // Serialize a JS value, and optionally transfer objects, into this message.
  // The Message object retains ownership of all transferred objects until
  // deserialization.
  // The source_port parameter, if provided, will make Serialize() throw a
  // "DataCloneError" DOMException if source_port is found in transfer_list.
  v8::Maybe<bool> Serialize(Environment* env,
                            v8::Local<v8::Context> context,
                            v8::Local<v8::Value> input,
                            const TransferList& transfer_list,
                            v8::Local<v8::Object> source_port =
                                v8::Local<v8::Object>());

  // Internal method of Message that is called when a new SharedArrayBuffer
  // object is encountered in the incoming value's structure.
  void AddSharedArrayBuffer(std::shared_ptr<v8::BackingStore> backing_store);
  // Internal method of Message that is called once serialization finishes
  // and that transfers ownership of `data` to this message.
  void AddMessagePort(std::unique_ptr<MessagePortData>&& data);
  // Internal method of Message that is called when a new WebAssembly.Module
  // object is encountered in the incoming value's structure.
  uint32_t AddWASMModule(v8::CompiledWasmModule&& mod);

  // The MessagePorts that will be transferred, as recorded by Serialize().
  // Used for warning user about posting the target MessagePort to itself,
  // which will as a side effect destroy the communication channel.
  const std::vector<std::unique_ptr<MessagePortData>>& message_ports() const {
    return message_ports_;
  }

  void MemoryInfo(MemoryTracker* tracker) const override;

  SET_MEMORY_INFO_NAME(Message)
  SET_SELF_SIZE(Message)

 private:
  MallocedBuffer<char> main_message_buf_;
  std::vector<std::shared_ptr<v8::BackingStore>> array_buffers_;
  std::vector<std::shared_ptr<v8::BackingStore>> shared_array_buffers_;
  std::vector<std::unique_ptr<MessagePortData>> message_ports_;
  std::vector<v8::CompiledWasmModule> wasm_modules_;

  friend class MessagePort;
};

// This contains all data for a `MessagePort` instance that is not tied to
// a specific Environment/Isolate/event loop, for easier transfer between those.
class MessagePortData : public MemoryRetainer {
 public:
  explicit MessagePortData(MessagePort* owner);
  ~MessagePortData() override;

  MessagePortData(MessagePortData&& other) = delete;
  MessagePortData& operator=(MessagePortData&& other) = delete;
  MessagePortData(const MessagePortData& other) = delete;
  MessagePortData& operator=(const MessagePortData& other) = delete;

  // Add a message to the incoming queue and notify the receiver.
  // This may be called from any thread.
  void AddToIncomingQueue(Message&& message);

  // Turns `a` and `b` into siblings, i.e. connects the sending side of one
  // to the receiving side of the other. This is not thread-safe.
  static void Entangle(MessagePortData* a, MessagePortData* b);

  // Removes any possible sibling. This is thread-safe (it acquires both
  // `sibling_mutex_` and `mutex_`), and has to be because it is called once
  // the corresponding JS handle handle wants to close
  // which can happen on either side of a worker.
  void Disentangle();

  void MemoryInfo(MemoryTracker* tracker) const override;

  SET_MEMORY_INFO_NAME(MessagePortData)
  SET_SELF_SIZE(MessagePortData)

 private:
  // This mutex protects all fields below it, with the exception of
  // sibling_.
  mutable Mutex mutex_;
  std::list<Message> incoming_messages_;
  MessagePort* owner_ = nullptr;
  // This mutex protects the sibling_ field and is shared between two entangled
  // MessagePorts. If both mutexes are acquired, this one needs to be
  // acquired first.
  std::shared_ptr<Mutex> sibling_mutex_ = std::make_shared<Mutex>();
  MessagePortData* sibling_ = nullptr;

  friend class MessagePort;
};

// A message port that receives messages from other threads, including
// the uv_async_t handle that is used to notify the current event loop of
// new incoming messages.
class MessagePort : public HandleWrap {
 public:
  // Create a new MessagePort. The `context` argument specifies the Context
  // instance that is used for creating the values emitted from this port.
  MessagePort(Environment* env,
              v8::Local<v8::Context> context,
              v8::Local<v8::Object> wrap);
  ~MessagePort() override;

  // Create a new message port instance, optionally over an existing
  // `MessagePortData` object.
  static MessagePort* New(Environment* env,
                          v8::Local<v8::Context> context,
                          std::unique_ptr<MessagePortData> data = nullptr);

  // Send a message, i.e. deliver it into the sibling's incoming queue.
  // If this port is closed, or if there is no sibling, this message is
  // serialized with transfers, then silently discarded.
  v8::Maybe<bool> PostMessage(Environment* env,
                              v8::Local<v8::Value> message,
                              const TransferList& transfer);

  // Start processing messages on this port as a receiving end.
  void Start();
  // Stop processing messages on this port as a receiving end.
  void Stop();

  /* constructor */
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  /* prototype methods */
  static void PostMessage(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Start(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Stop(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Drain(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReceiveMessage(const v8::FunctionCallbackInfo<v8::Value>& args);

  /* static */
  static void MoveToContext(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Turns `a` and `b` into siblings, i.e. connects the sending side of one
  // to the receiving side of the other. This is not thread-safe.
  static void Entangle(MessagePort* a, MessagePort* b);
  static void Entangle(MessagePort* a, MessagePortData* b);

  // Detach this port's data for transferring. After this, the MessagePortData
  // is no longer associated with this handle, although it can still receive
  // messages.
  std::unique_ptr<MessagePortData> Detach();

  void Close(
      v8::Local<v8::Value> close_callback = v8::Local<v8::Value>()) override;

  // Returns true if either data_ has been freed, or if the handle is being
  // closed. Equivalent to the [[Detached]] internal slot in the HTML Standard.
  //
  // If checking if a JavaScript MessagePort object is detached, this method
  // alone is often not enough, since the backing C++ MessagePort object may
  // have been deleted already. For all intents and purposes, an object with a
  // NULL pointer to the C++ MessagePort object is also detached.
  inline bool IsDetached() const;

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("data", data_);
  }

  SET_MEMORY_INFO_NAME(MessagePort)
  SET_SELF_SIZE(MessagePort)

 private:
  void OnClose() override;
  void OnMessage();
  void TriggerAsync();
  v8::MaybeLocal<v8::Value> ReceiveMessage(v8::Local<v8::Context> context,
                                           bool only_if_receiving);

  std::unique_ptr<MessagePortData> data_ = nullptr;
  bool receiving_messages_ = false;
  uv_async_t async_;

  friend class MessagePortData;
};

v8::Local<v8::FunctionTemplate> GetMessagePortConstructorTemplate(
    Environment* env);

}  // namespace worker
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS


#endif  // SRC_NODE_MESSAGING_H_
