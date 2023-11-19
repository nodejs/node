#ifndef ENVPAIR_H
#define ENVPAIR_H
#include "EnvKey.h"
#include "EnvValue.h"
namespace cppnv {
struct EnvPair {
  EnvKey* key;
  EnvValue* value;
};
}
#endif  // ENVPAIR_H
