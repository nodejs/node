#ifndef SRC_DIAGNOSTICFILENAME_INL_H_
#define SRC_DIAGNOSTICFILENAME_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_internals.h"
namespace node {

class Environment;

inline DiagnosticFilename::DiagnosticFilename(
    Environment* env,
    const char* prefix,
    const char* ext) :
  filename_(MakeFilename(env->thread_id(), prefix, ext)) {
}

inline DiagnosticFilename::DiagnosticFilename(
    uint64_t thread_id,
    const char* prefix,
    const char* ext) :
  filename_(MakeFilename(thread_id, prefix, ext)) {
}

inline const char* DiagnosticFilename::operator*() const {
  return filename_.c_str();
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_DIAGNOSTICFILENAME_INL_H_
