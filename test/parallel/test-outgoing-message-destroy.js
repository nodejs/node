'use strict';

// Test that http.OutgoingMessage,prototype.destroy() returns `this`.
require('../common');

const assert = require('assert');
const http = require('http');
const outgoingMessage = new http.OutgoingMessage();

assert.strictEqual(outgoingMessage.destroyed, false);
assert.strictEqual(outgoingMessage.destroy(), outgoingMessage);
assert.strictEqual(outgoingMessage.destroyed, true);
assert.strictEqual(outgoingMessage.destroy(), outgoingMessage);
