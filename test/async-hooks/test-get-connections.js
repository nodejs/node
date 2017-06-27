'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');
const server = net.createServer();

/* This test was based on an error raised by Haraka.
   It caused server.getConnections to raise an exception.
   
   See: https://github.com/haraka/Haraka/pull/1951
*/

server.getConnections(common.mustCall());
