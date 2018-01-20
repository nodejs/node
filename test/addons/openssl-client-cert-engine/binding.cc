#include "node.h"
#include "v8.h"

inline void Initialize(v8::Local<v8::Object>) {}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
