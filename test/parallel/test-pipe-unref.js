'use strict';
const common = require('../common');
const net = require('net');

common.refreshTmpDir();

var s = net.Server();
s.listen(common.PIPE);
s.unref();

setTimeout(common.fail, 1000).unref();
