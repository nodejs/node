'use strict';
const common = require('../common');

const { OutgoingMessage } = require('http');
const assert = require('assert');

const warn = 'OutgoingMessage.prototype._headerNames is deprecated';
common.expectWarning('DeprecationWarning', warn, 'DEP0066');

{
  // Tests for _headerNames get method
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage._headerNames; // eslint-disable-line no-unused-expressions
}

{
  // Tests _headerNames getter result after setting a header.
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader('key', 'value');
  const expect = { __proto__: null };
  expect.key = 'key';
  assert.deepStrictEqual(outgoingMessage._headerNames, expect);
}
