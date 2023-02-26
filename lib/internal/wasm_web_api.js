'use strict';

const {
  PromiseResolve,
} = primordials;
const {
  ERR_INVALID_ARG_TYPE,
  ERR_WEBASSEMBLY_RESPONSE,
} = require('internal/errors').codes;

let undici;
function lazyUndici() {
  return undici ??= require('internal/deps/undici/undici');
}

// This is essentially an implementation of a v8::WasmStreamingCallback, except
// that it is implemented in JavaScript because the fetch() implementation is
// difficult to use from C++. See lib/internal/process/pre_execution.js and
// src/node_wasm_web_api.cc that interact with this function.
function wasmStreamingCallback(streamState, source) {
  (async () => {
    const response = await PromiseResolve(source);
    if (!(response instanceof lazyUndici().Response)) {
      throw new ERR_INVALID_ARG_TYPE(
        'source', ['Response', 'Promise resolving to Response'], response);
    }

    const contentType = response.headers.get('Content-Type');
    if (contentType !== 'application/wasm') {
      throw new ERR_WEBASSEMBLY_RESPONSE(
        `has unsupported MIME type '${contentType}'`);
    }

    if (!response.ok) {
      throw new ERR_WEBASSEMBLY_RESPONSE(
        `has status code ${response.status}`);
    }

    if (response.bodyUsed !== false) {
      throw new ERR_WEBASSEMBLY_RESPONSE('body has already been used');
    }

    if (response.url) {
      streamState.setURL(response.url);
    }

    // Pass all data from the response body to the WebAssembly compiler.
    const { body } = response;
    if (body != null) {
      for await (const chunk of body) {
        streamState.push(chunk);
      }
    }
  })().then(() => {
    // No error occurred. Tell the implementation that the stream has ended.
    streamState.finish();
  }, (err) => {
    // An error occurred, either because the given object was not a valid
    // and usable Response or because a network error occurred.
    streamState.abort(err);
  });
}

module.exports = {
  wasmStreamingCallback,
};
