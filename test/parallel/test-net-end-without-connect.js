'use strict';
require('../common');
const net = require('net');

const sock = new net.Socket();
sock.end();  // Should not throw.
