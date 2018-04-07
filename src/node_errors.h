#ifndef SRC_NODE_ERRORS_H_
#define SRC_NODE_ERRORS_H_

#include "v8.h"

namespace node {
namespace errors {

void PrintErrorString(const char*, ...);

class NodeTryCatch : public v8::TryCatch {
 public:
  explicit NodeTryCatch(Environment* env, bool fatal = false) :
      v8::TryCatch(env->isolate()), env_(env), fatal_(fatal) {}
  ~NodeTryCatch();

  v8::Local<v8::Value> ReThrow(bool decorate = false);

 private:
  Environment* env_;
  bool fatal_;
};

void ReportException(
    Environment*, v8::Local<v8::Value>, v8::Local<v8::Message>);

void ReportException(Environment*, const NodeTryCatch&);

}  // namespace errors
}  // namespace node

#endif  // SRC_NODE_ERRORS_H_
