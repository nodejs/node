#ifndef SRC_PERSISTENT_HANDLE_CLEANUP_H_
#define SRC_PERSISTENT_HANDLE_CLEANUP_H_

#include "v8.h"
#include "env.h"

namespace node {

class PersistentHandleCleanup : public v8::PersistentHandleVisitor {
  public:
    void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                               uint16_t class_id) override;
  private:
    explicit PersistentHandleCleanup(Environment* env) : env_(env) {}

    Environment* env() const {
      return env_;
    }

    Environment* const env_;
    friend class Environment;
    DISALLOW_COPY_AND_ASSIGN(PersistentHandleCleanup);
};
}  // namespace node

#endif  // SRC_PERSISTENT_HANDLE_CLEANUP_H_
