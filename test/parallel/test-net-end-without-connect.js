'use strict';
var common = require('../common');
var net = require('net');

var sock = new net.Socket();
sock.end();  // Should not throw.
