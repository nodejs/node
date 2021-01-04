#ifndef SRC_NODE_WASM_STREAMING_H_
#define SRC_NODE_WASM_STREAMING_H_

#include "node.h"
#include "v8.h"

namespace node {
namespace WasmStreaming {

// Implements streaming for WebAssembly.compileStreaming (V8 callback)
// by forwarding the call to a user-registered JS function.
void WasmStreamingCallback(const v8::FunctionCallbackInfo<v8::Value>& args);

}  // namespace WasmStreaming
}  // namespace node

#endif  // SRC_NODE_WASM_STREAMING_H_
