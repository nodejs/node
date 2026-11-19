#ifndef SRC_NODE_JSON_EVENTS_H_
#define SRC_NODE_JSON_EVENTS_H_

#include <string_view>

namespace node {
namespace json {

// Receives one depth-first walk of a JSON document. Implemented in baseline
// code so that the per-ISA Jsonifier translation units only ever pass
// std::string_view and scalars across this boundary.
class JsonEventSink {
 public:
  virtual ~JsonEventSink();
  virtual void StartObject() = 0;
  virtual void EndObject(std::string_view raw) = 0;
  virtual void StartArray() = 0;
  virtual void EndArray(std::string_view raw) = 0;
  virtual void Key(std::string_view key) = 0;
  virtual void String(std::string_view value) = 0;
  virtual void Number(std::string_view text, double value) = 0;
  virtual void Bool(bool value) = 0;
  virtual void Null() = 0;
};

// Parses `json` with the Jsonifier tier selected for this CPU and reports the
// whole document to `sink`. Returns an empty view on success, or the name of
// the Jsonifier error that stopped the walk.
std::string_view WalkJson(std::string_view json, JsonEventSink* sink);

}  // namespace json
}  // namespace node

#endif  // SRC_NODE_JSON_EVENTS_H_
