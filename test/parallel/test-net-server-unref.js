'use strict';
const common = require('../common');
const net = require('net');

const s = net.createServer();
s.listen(0);
s.unref();

setTimeout(common.mustNotCall(), 1000).unref();
