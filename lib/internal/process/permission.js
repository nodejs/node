'use strict';

const {
  Boolean,
  ObjectFreeze,
} = primordials;

const permission = internalBinding('permission');
const { validateString, validateBuffer } = require('internal/validators');
const { Buffer } = require('buffer');
const { isBuffer } = Buffer;

let _permission;
const hasFFI = Boolean(process.config.variables.node_use_ffi);

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

    if (hasFFI) {
      flags.splice(4, 0, '--allow-ffi');
    }

    return flags;
  },
});
