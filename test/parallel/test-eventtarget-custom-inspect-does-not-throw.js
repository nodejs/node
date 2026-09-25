'use strict';

require('../common');
const assert = require('assert');
const util = require('util');

const symbol = util.inspect.custom;

const eventTargetInspect = EventTarget.prototype[symbol];

const fakeEventTarget = {
  [symbol]: eventTargetInspect,
  someOtherField: 42
};

// Should not throw when calling the custom inspect method
const output = util.inspect(fakeEventTarget);

assert.strictEqual(typeof output, 'string');
