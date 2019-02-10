'use strict';

// This is only exposed for internal build steps and testing purposes.
// We create new copies of the source and the code cache
// so the resources eventually used to compile builtin modules
// cannot be tampered with even with --expose-internals.

const { NativeModule } = require('internal/bootstrap/loaders');
const {
  getCodeCache, compileFunction
} = internalBinding('native_module');
const { hasTracing, hasInspector } = internalBinding('config');

// Modules with source code compiled in js2c that
// cannot be compiled with the code cache.
const cannotBeRequired = [
  'sys',  // Deprecated.
  'internal/v8_prof_polyfill',
  'internal/v8_prof_processor',

  'internal/test/binding',

  'internal/bootstrap/context',
  'internal/bootstrap/primordials',
  'internal/bootstrap/loaders',
  'internal/bootstrap/node'
];

// Skip modules that cannot be required when they are not
// built into the binary.
if (!hasInspector) {
  cannotBeRequired.push(
    'inspector',
    'internal/util/inspector',
  );
}
if (!hasTracing) {
  cannotBeRequired.push('trace_events');
}
if (!process.versions.openssl) {
  cannotBeRequired.push(
    'crypto',
    'https',
    'http2',
    'tls',
    '_tls_common',
    '_tls_wrap',
    'internal/crypto/certificate',
    'internal/crypto/cipher',
    'internal/crypto/diffiehellman',
    'internal/crypto/hash',
    'internal/crypto/keygen',
    'internal/crypto/keys',
    'internal/crypto/pbkdf2',
    'internal/crypto/random',
    'internal/crypto/scrypt',
    'internal/crypto/sig',
    'internal/crypto/util',
    'internal/http2/core',
    'internal/http2/compat',
    'internal/policy/manifest',
    'internal/process/policy',
    'internal/streams/lazy_transform',
  );
}

const cachableBuiltins = [];
for (const id of NativeModule.map.keys()) {
  if (id.startsWith('internal/deps') || id.startsWith('internal/main')) {
    cannotBeRequired.push(id);
  }
  if (!cannotBeRequired.includes(id)) {
    cachableBuiltins.push(id);
  }
}

module.exports = {
  cachableBuiltins,
  getCodeCache,
  compileFunction,
  cannotBeRequired
};
