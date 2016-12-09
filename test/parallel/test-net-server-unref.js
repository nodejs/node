'use strict';
const common = require('../common');
var net = require('net');

var s = net.createServer();
s.listen(0);
s.unref();

setTimeout(common.fail, 1000).unref();
