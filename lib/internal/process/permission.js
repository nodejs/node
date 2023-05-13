'use strict';

const {
  ObjectFreeze,
  RegExpPrototypeExec,
  StringPrototypeStartsWith,
} = primordials;

const permission = internalBinding('permission');
const { validateString } = require('internal/validators');
const { isAbsolute, resolve } = require('path');
const {
  codes: { ERR_INVALID_ARG_VALUE },
} = require('internal/errors');

let experimentalPermission;

function validateHasEnvReference(value, name) {
  if (!RegExpPrototypeExec(/^[a-zA-Z_][a-zA-Z0-9_]*$/, value)) {
    throw new ERR_INVALID_ARG_VALUE(
      name,
      value,
      'must start with a letter or underscore, follewed by letters, digits, or underscores',
    );
  }
}

module.exports = ObjectFreeze({
  __proto__: null,
  isEnabled() {
    if (experimentalPermission === undefined) {
      const { getOptionValue } = require('internal/options');
      experimentalPermission = getOptionValue('--experimental-permission');
    }
    return experimentalPermission;
  },
  has(scope, reference) {
    validateString(scope, 'scope');
    if (reference != null) {
      // TODO: add support for WHATWG URLs and Uint8Arrays.
      validateString(reference, 'reference');
      if (StringPrototypeStartsWith(scope, 'fs')) {
        if (!isAbsolute(reference)) {
          reference = resolve(reference);
        }
      } else if (StringPrototypeStartsWith(scope, 'env')) {
        validateHasEnvReference(reference, 'reference');
      }
    }

    return permission.has(scope, reference);
  },
});
