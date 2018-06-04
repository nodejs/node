#include "tracing_agent.h"

#include "env-inl.h"
#include "v8.h"

#include <set>
#include <sstream>

namespace node {
namespace inspector {
namespace protocol {

namespace {
using v8::platform::tracing::TraceWriter;

class InspectorTraceWriter : public node::tracing::AsyncTraceWriter {
 public:
  explicit InspectorTraceWriter(NodeTracing::Frontend* frontend)
                                : frontend_(frontend) {}

  void AppendTraceEvent(
      v8::platform::tracing::TraceObject* trace_event) override {
    if (!json_writer_)
      json_writer_.reset(TraceWriter::CreateJSONTraceWriter(stream_, "value"));
    json_writer_->AppendTraceEvent(trace_event);
  }

  void Flush(bool) override {
    if (!json_writer_)
      return;
    json_writer_.reset();
    std::ostringstream result(
        "{\"method\":\"NodeTracing.dataCollected\",\"data\":",
        std::ostringstream::ate);
    result << stream_.str();
    result << "}";
    frontend_->sendRawNotification(result.str());
    stream_.str("");
  }

 private:
  std::unique_ptr<TraceWriter> json_writer_;
  std::ostringstream stream_;
  NodeTracing::Frontend* frontend_;
};
}  // namespace

TracingAgent::TracingAgent(Environment* env)
                           : env_(env),
                             trace_writer_(
                                  tracing::Agent::EmptyClientHandle()) {
}

TracingAgent::~TracingAgent() {
  trace_writer_.reset();
}

void TracingAgent::Wire(UberDispatcher* dispatcher) {
  frontend_.reset(new NodeTracing::Frontend(dispatcher->channel()));
  NodeTracing::Dispatcher::wire(dispatcher, this);
}

DispatchResponse TracingAgent::start(
    std::unique_ptr<protocol::NodeTracing::TraceConfig> traceConfig) {
  if (trace_writer_ != nullptr) {
    return DispatchResponse::Error(
        "Call NodeTracing::end to stop tracing before updating the config");
  }

  std::set<std::string> categories_set;
  protocol::Array<std::string>* categories =
      traceConfig->getIncludedCategories();
  for (size_t i = 0; i < categories->length(); i++)
    categories_set.insert(categories->get(i));

  if (categories_set.empty())
    return DispatchResponse::Error("At least one category should be enabled");

  trace_writer_ = env_->tracing_agent()->AddClient(
      categories_set,
      std::unique_ptr<InspectorTraceWriter>(
          new InspectorTraceWriter(frontend_.get())));
  return DispatchResponse::OK();
}

DispatchResponse TracingAgent::stop() {
  trace_writer_.reset();
  frontend_->tracingComplete();
  return DispatchResponse::OK();
}

DispatchResponse TracingAgent::getCategories(
    std::unique_ptr<protocol::Array<String>>* categories) {
  *categories = Array<String>::create();
  categories->get()->addItem("node");
  categories->get()->addItem("node.async");
  categories->get()->addItem("node.bootstrap");
  categories->get()->addItem("node.fs.sync");
  categories->get()->addItem("node.perf");
  categories->get()->addItem("node.perf.usertiming");
  categories->get()->addItem("node.perf.timerify");
  categories->get()->addItem("v8");
  return DispatchResponse::OK();
}

}  // namespace protocol
}  // namespace inspector
}  // namespace node
