'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');

// Connect to something that we need to DNS resolve
var c = net.createConnection(80, 'google.com');
c.destroy();
