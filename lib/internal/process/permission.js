'use strict';

const {
  ArrayPrototypePush,
  ObjectFreeze,
} = primordials;

const permission = internalBinding('permission');
const { validateString, validateBuffer } = require('internal/validators');
const { Buffer } = require('buffer');
const { isBuffer } = Buffer;

let _permission;
let _ffi;

module.exports = ObjectFreeze({
  __proto__: null,
  isEnabled() {
    if (_permission === undefined) {
      const { getOptionValue } = require('internal/options');
      _permission = getOptionValue('--permission');
    }
    return _permission;
  },
  has(scope, reference) {
    validateString(scope, 'scope');
    if (reference != null) {
      // TODO: add support for WHATWG URLs and Uint8Arrays.
      if (isBuffer(reference)) {
        validateBuffer(reference, 'reference');
      } else {
        validateString(reference, 'reference');
      }
    }

    return permission.has(scope, reference);
  },
  availableFlags() {
    if (_ffi === undefined) {
      const { getOptionValue } = require('internal/options');
      _ffi = getOptionValue('--experimental-ffi');
    }

    const flags = [
      '--allow-fs-read',
      '--allow-fs-write',
      '--allow-addons',
      '--allow-child-process',
      '--allow-net',
      '--allow-inspector',
      '--allow-wasi',
      '--allow-worker',
    ];

    if (_ffi) {
      ArrayPrototypePush(flags, '--allow-ffi');
    }

    return flags;
  },
});
