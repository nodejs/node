'use strict';

const common = require('../common');
const net = require('net');
const server = net.createServer();

// This test was based on an error raised by Haraka.
// It caused server.getConnections to raise an exception.
// Ref: https://github.com/haraka/Haraka/pull/1951

server.getConnections(common.mustCall());
