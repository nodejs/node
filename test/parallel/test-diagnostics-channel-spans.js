'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const inputs = [
  { test: 'start' },
  { test: 'annotation' },
  { test: 'end' }
];

const channel = dc.channel('test');

// Span interface produces inactive spans when not subscribing.
const inactiveSpan = channel.span({});
assert.strictEqual(inactiveSpan.channel, undefined);

let expectedAction;
let expectedId = 0;
let expectedMessage;

channel.subscribe(common.mustCall((message) => {
  assert.strictEqual(message.toJSON().type, 'span');
  assert.strictEqual(message.action, expectedAction);
  assert.strictEqual(message.id, 2 - (++expectedId % 2));
  assert.deepStrictEqual(message.data, expectedMessage);
}, inputs.length * 2));

expectedAction = 'start';
expectedMessage = inputs.shift();
const spanA = channel.span(expectedMessage);
const spanB = channel.span(expectedMessage);

expectedAction = 'annotation';
expectedMessage = inputs.shift();
spanA.annotate(expectedMessage);
spanB.annotate(expectedMessage);

expectedAction = 'end';
expectedMessage = inputs.shift();
spanA.end(expectedMessage);
spanB.end(expectedMessage);
