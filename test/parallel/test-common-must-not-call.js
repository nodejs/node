'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const util = require('util');
const { fileURLToPath } = require('url');

const message = 'message';
const testFunction1 = common.mustNotCall(message);

const testFunction2 = common.mustNotCall(message);

const createValidate = (line, args = []) => common.mustCall((e) => {
  const prefix = `${message} at `;
  assert.ok(e.message.startsWith(prefix));
  e.message = e.message.substring(prefix.length);
  if (e.message.startsWith('file:')) {
    const url = /.*/.exec(e.message)[0];
    e.message = fileURLToPath(url) + e.message.substring(url.length);
  }
  if (process.platform === 'win32') {
    e.message = e.message.substring(2); // remove 'C:'
  }
  const msg = e.message;
  const firstColon = msg.indexOf(':');
  const fileName = msg.substring(0, firstColon);
  const rest = msg.substring(firstColon + 1);
  assert.strictEqual(path.basename(fileName), 'test-common-must-not-call.js');
  const argsInfo = args.length > 0 ?
    `\ncalled with arguments: ${args.map(util.inspect).join(', ')}` : '';
  assert.strictEqual(rest, line + argsInfo);
});

const validate1 = createValidate('10');
try {
  testFunction1();
} catch (e) {
  validate1(e);
}

const validate2 = createValidate('12', ['hello', 42]);
try {
  testFunction2('hello', 42);
} catch (e) {
  validate2(e);
}
