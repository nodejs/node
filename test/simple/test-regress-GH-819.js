var common = require('../common');
var net = require('net');
var assert = require('assert');

// Connect to something that we need to DNS resolve
var c = net.createConnection(80, "google.com");
c.destroy();
