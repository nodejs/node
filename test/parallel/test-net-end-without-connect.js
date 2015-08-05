'use strict';
const common = require('../common');
const net = require('net');

var sock = new net.Socket();
sock.end();  // Should not throw.
