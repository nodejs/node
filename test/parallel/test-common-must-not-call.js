'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const util = require('util');

const message = 'message';
const testFunction1 = common.mustNotCall(message);

const testFunction2 = common.mustNotCall(message);

const createValidate = (line, args = []) => common.mustCall((e) => {
  const prefix = `${message} at `;
  assert.ok(e.message.startsWith(prefix));
  if (process.platform === 'win32') {
    e.message = e.message.substring(2); // remove 'C:'
  }
  const msg = e.message.substring(prefix.length);
  const firstColon = msg.indexOf(':');
  const fileName = msg.substring(0, firstColon);
  const rest = msg.substring(firstColon + 1);
  assert.strictEqual(path.basename(fileName), 'test-common-must-not-call.js');
  const argsInfo = args.length > 0 ?
    `\ncalled with arguments: ${args.map(util.inspect).join(', ')}` : '';
  assert.strictEqual(rest, line + argsInfo);
});

const validate1 = createValidate('9');
try {
  testFunction1();
} catch (e) {
  validate1(e);
}

const validate2 = createValidate('11', ['hello', 42]);
try {
  testFunction2('hello', 42);
} catch (e) {
  validate2(e);
}

assert.throws(
  () => new Proxy({ prop: Symbol() }, { get: common.mustNotCall() }).prop,
  { code: 'ERR_ASSERTION' }
);

{
  const { inspect } = util;
  delete util.inspect;
  assert.throws(
    () => common.mustNotCall()(null),
    { code: 'ERR_ASSERTION' }
  );
  util.inspect = inspect;
}
