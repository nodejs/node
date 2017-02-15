'use strict';
const common = require('../common');
const net = require('net');
const server = net.createServer();

// unref before listening
server.unref();
server.listen();

// If the timeout fires, that means the server held the event loop open
// and the unref() was not persistent. Close the server and fail the test.
setTimeout(common.mustNotCall(), 1000).unref();
