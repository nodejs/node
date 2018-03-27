'use strict';
require('../common');

// This test ensures that `net.Socket` uses the `skipChunkCheck` option
// when calling the `Readable` constructor.

const { Socket } = require('net');
const { strictEqual } = require('assert');

const socket = new Socket();
strictEqual(socket._readableState.skipChunkCheck, true);
