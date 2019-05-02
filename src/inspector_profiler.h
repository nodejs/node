#ifndef SRC_INSPECTOR_PROFILER_H_
#define SRC_INSPECTOR_PROFILER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif

#include "env.h"
#include "inspector_agent.h"

namespace node {
// Forward declaration to break recursive dependency chain with src/env.h.
class Environment;

namespace profiler {

class V8ProfilerConnection {
 public:
  class V8ProfilerSessionDelegate : public inspector::InspectorSessionDelegate {
   public:
    explicit V8ProfilerSessionDelegate(V8ProfilerConnection* connection)
        : connection_(connection) {}

    void SendMessageToFrontend(
        const v8_inspector::StringView& message) override;

   private:
    V8ProfilerConnection* connection_;
  };

  explicit V8ProfilerConnection(Environment* env);
  virtual ~V8ProfilerConnection() = default;

  Environment* env() const { return env_; }

  // Dispatch a protocol message, and returns the id of the message.
  // `method` does not need to be surrounded by quotes.
  // The optional `params` should be formatted in JSON.
  // The strings should be in one byte characters - which is enough for
  // the commands we use here.
  size_t DispatchMessage(const char* method, const char* params = nullptr);

  // Use DispatchMessage() to dispatch necessary inspector messages
  // to start and end the profiling.
  virtual void Start() = 0;
  virtual void End() = 0;

  // Return a descriptive name of the profile for debugging.
  virtual const char* type() const = 0;
  // Return if the profile is ending and the response can be parsed.
  virtual bool ending() const = 0;
  // Return the directory where the profile should be placed.
  virtual std::string GetDirectory() const = 0;
  // Return the filename the profile should be written as.
  virtual std::string GetFilename() const = 0;
  // Return the profile object parsed from `message.result`,
  // which will be then written as a JSON.
  virtual v8::MaybeLocal<v8::Object> GetProfile(
      v8::Local<v8::Object> result) = 0;

 private:
  size_t next_id() { return id_++; }
  void WriteProfile(v8::Local<v8::String> message);
  std::unique_ptr<inspector::InspectorSession> session_;
  Environment* env_ = nullptr;
  size_t id_ = 1;
};

class V8CoverageConnection : public V8ProfilerConnection {
 public:
  explicit V8CoverageConnection(Environment* env) : V8ProfilerConnection(env) {}

  void Start() override;
  void End() override;

  const char* type() const override { return type_.c_str(); }
  bool ending() const override { return ending_; }

  std::string GetDirectory() const override;
  std::string GetFilename() const override;
  v8::MaybeLocal<v8::Object> GetProfile(v8::Local<v8::Object> result) override;

 private:
  std::unique_ptr<inspector::InspectorSession> session_;
  bool ending_ = false;
  std::string type_ = "coverage";
};

class V8CpuProfilerConnection : public V8ProfilerConnection {
 public:
  explicit V8CpuProfilerConnection(Environment* env)
      : V8ProfilerConnection(env) {}

  void Start() override;
  void End() override;

  const char* type() const override { return type_.c_str(); }
  bool ending() const override { return ending_; }

  std::string GetDirectory() const override;
  std::string GetFilename() const override;
  v8::MaybeLocal<v8::Object> GetProfile(v8::Local<v8::Object> result) override;

 private:
  std::unique_ptr<inspector::InspectorSession> session_;
  bool ending_ = false;
  std::string type_ = "CPU";
};

}  // namespace profiler
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_INSPECTOR_PROFILER_H_
