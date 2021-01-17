// Flags: --expose-internals
'use strict';

const common = require('../common');
const { primordials } = require('internal/test/binding');
const {
  SafeMap,
} = primordials;

const { options, aliases, getOptionValue } = require('internal/options');
const assert = require('assert');

assert(options instanceof SafeMap,
       "require('internal/options').options is a SafeMap");

assert(aliases instanceof SafeMap,
       "require('internal/options').aliases is a SafeMap");

Map.prototype.get =
  common.mustNotCall('%Map.prototype.get% must not be called');
assert.strictEqual(getOptionValue('--expose-internals'), true);
