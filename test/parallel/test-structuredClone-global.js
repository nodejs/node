// Flags: --expose-internals
'use strict';
/* eslint-disable no-global-assign */

require('../common');

const {
  structuredClone: _structuredClone,
} = require('internal/structured_clone');

const {
  strictEqual,
  throws,
} = require('assert');

strictEqual(globalThis.structuredClone, _structuredClone);
structuredClone = undefined;
strictEqual(globalThis.structuredClone, undefined);

// Restore the value for the known globals check.
structuredClone = _structuredClone;

throws(() => _structuredClone(), /ERR_MISSING_ARGS/);
