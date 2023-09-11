'use strict';

// Test that http.IncomingMessage,prototype.destroy() returns `this`.
require('../common');

const assert = require('assert');
const http = require('http');
const incomingMessage = new http.IncomingMessage();

assert.strictEqual(incomingMessage.destroy(), incomingMessage);
