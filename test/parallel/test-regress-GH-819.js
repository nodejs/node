'use strict';
require('../common');
const net = require('net');

// Connect to something that we need to DNS resolve
const c = net.createConnection(80, 'google.com');
c.destroy();
