'use strict';

const common = require('../common');
const { once, EventEmitter: EE } = require('events');
const { strictEqual, deepStrictEqual } = require('assert');

{
  const e = new EE();
  e.addListener = common.mustCall();
  e.on('test', () => {});
}

{
  const e = new EE();
  e.removeListener = common.mustCall();
  e.off('test', () => {});
}
