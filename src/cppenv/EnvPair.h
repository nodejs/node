#pragma once
#include "EnvKey.h"
#include "EnvValue.h"

struct env_pair
{
    env_key *key;
    env_value *value;
};
