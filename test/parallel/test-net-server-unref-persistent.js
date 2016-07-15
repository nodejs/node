'use strict';
const common = require('../common');
var net = require('net');
var server = net.createServer();

// unref before listening
server.unref();
server.listen();

// If the timeout fires, that means the server held the event loop open
// and the unref() was not persistent. Close the server and fail the test.
setTimeout(common.fail, 1000).unref();
