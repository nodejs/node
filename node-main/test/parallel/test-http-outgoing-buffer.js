'use strict';
require('../common');
const assert = require('assert');
const { getDefaultHighWaterMark } = require('stream');

const http = require('http');
const OutgoingMessage = http.OutgoingMessage;

const msg = new OutgoingMessage();
msg._implicitHeader = function() {};

// Writes should be buffered until highwatermark
// even when no socket is assigned.

assert.strictEqual(msg.write('asd'), true);
while (msg.write('asd'));
const highwatermark = msg.writableHighWaterMark || getDefaultHighWaterMark();
assert(msg.outputSize >= highwatermark);
