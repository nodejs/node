'use strict';

const {
  ArrayPrototypePush,
  ObjectFreeze,
  StringPrototypeStartsWith,
} = primordials;

const permission = internalBinding('permission');
const { validateString } = require('internal/validators');
const { isUint8Array } = require('internal/util/types');
const { isURL, fileURLToPath } = require('internal/url');

let _permission;
let _audit;
let _ffi;

function normalizeReference(scope, reference, name) {
  if (reference == null) {
    return reference;
  }

  if (isURL(reference)) {
    // Reference is only meaningful for fs.* scopes (FSPermission does
    // per-path checks); every other scope ignores it (BooleanPermission),
    // so only fs.* scopes require a file: URL.
    if (StringPrototypeStartsWith(scope, 'fs')) {
      return fileURLToPath(reference);
    }
    return reference.href;
  }

  if (isUint8Array(reference)) {
    // Passed through as-is: the native binding copies the raw bytes
    // directly instead of forcing a (potentially lossy) UTF-8 string
    // conversion, since paths are not guaranteed to be valid UTF-8.
    return reference;
  }

  validateString(reference, name);
  return reference;
}

module.exports = ObjectFreeze({
  __proto__: null,
  isEnabled() {
    if (_permission === undefined) {
      const { getOptionValue } = require('internal/options');
      _permission = getOptionValue('--permission') || getOptionValue('--permission-audit');
    }
    return _permission;
  },
  isAuditMode() {
    if (_audit === undefined) {
      const { getOptionValue } = require('internal/options');
      _audit = getOptionValue('--permission-audit');
    }
    return _audit;
  },
  has(scope, reference) {
    validateString(scope, 'scope');
    return permission.has(scope, normalizeReference(scope, reference, 'reference'));
  },
  drop(scope, reference) {
    validateString(scope, 'scope');
    permission.drop(scope, normalizeReference(scope, reference, 'reference'));
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
      '--allow-openssl-store',
    ];

    if (_ffi) {
      ArrayPrototypePush(flags, '--allow-ffi');
    }

    return flags;
  },
});
