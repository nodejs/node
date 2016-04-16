'use strict';
require('../common');
var net = require('net');

// Connect to something that we need to DNS resolve
var c = net.createConnection(80, 'google.com');
c.destroy();
