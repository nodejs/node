'use strict';

const assert = require('assert');
const util = require('util');

const symbol = util.inspect.custom;

const eventTargetInspect = EventTarget.prototype[symbol];

const fakeEventTarget = {
  [symbol]: eventTargetInspect,
  someOtherField: 42
};

assert.doesNotThrow(() => {
  util.inspect(fakeEventTarget);
});
