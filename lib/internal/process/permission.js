'use strict';

const {
  ObjectFreeze,
  ArrayPrototypePush,
} = primordials;

const permission = internalBinding('permission');
const { validateString, validateArray } = require('internal/validators');
const { isAbsolute, resolve } = require('path');

let experimentalPermission;

module.exports = ObjectFreeze({
  __proto__: null,
  isEnabled() {
    if (experimentalPermission === undefined) {
      const { getOptionValue } = require('internal/options');
      experimentalPermission = getOptionValue('--experimental-permission');
    }
    return experimentalPermission;
  },
  deny(scope, references) {
    validateString(scope, 'scope');
    if (references == null) {
      return permission.deny(scope, references);
    }

    validateArray(references, 'references');
    // TODO(rafaelgss): change to call fs_permission.resolve when available
    const normalizedParams = [];
    for (let i = 0; i < references.length; ++i) {
      if (isAbsolute(references[i])) {
        ArrayPrototypePush(normalizedParams, references[i]);
      } else {
        // TODO(aduh95): add support for WHATWG URLs and Uint8Arrays.
        ArrayPrototypePush(normalizedParams, resolve(references[i]));
      }
    }

    return permission.deny(scope, normalizedParams);
  },

  has(scope, reference) {
    validateString(scope, 'scope');
    if (reference != null) {
      // TODO: add support for WHATWG URLs and Uint8Arrays.
      validateString(reference, 'reference');
      if (!isAbsolute(reference)) {
        return permission.has(scope, resolve(reference));
      }
    }

    return permission.has(scope, reference);
  },
});
