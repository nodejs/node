#ifndef ENVPAIR_H
#define ENVPAIR_H
#include "EnvKey.h"
#include "EnvValue.h"
namespace cppnv {
struct env_pair
{
  env_key *key;
  env_value *value;
};
}
#endif // ENVPAIR_H
