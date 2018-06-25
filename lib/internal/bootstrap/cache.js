'use strict';

// This is only exposed for internal build steps and testing purposes.
// We create new copies of the source and the code cache
// so the resources eventually used to compile builtin modules
// cannot be tampered with even with --expose-internals

const {
  NativeModule, internalBinding
} = require('internal/bootstrap/loaders');

module.exports = {
  builtinSource: Object.assign({}, NativeModule._source),
  codeCache: internalBinding('code_cache'),
  compiledWithoutCache: NativeModule.compiledWithoutCache,
  compiledWithCache: NativeModule.compiledWithCache,
  nativeModuleWrap(script) {
    return NativeModule.wrap(script);
  },
  // Modules with source code compiled in js2c that
  // cannot be compiled with the code cache
  cannotUseCache: [
    'config',
    // TODO(joyeecheung): update the C++ side so that
    // the code cache is also used when compiling these
    // two files.
    'internal/bootstrap/loaders',
    'internal/bootstrap/node',
    'internal/per_context',
  ]
};
