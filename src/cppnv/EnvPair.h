#ifndef SRC_CPPNV_ENVPAIR_H_
#define SRC_CPPNV_ENVPAIR_H_
#include "EnvKey.h"
#include "EnvValue.h"
namespace cppnv {
struct EnvPair {
  EnvKey* key;
  EnvValue* value;
};
}
#endif  // SRC_CPPNV_ENVPAIR_H_
