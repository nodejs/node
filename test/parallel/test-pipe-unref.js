'use strict';
var common = require('../common');
var net = require('net');

common.refreshTmpDir();

var s = net.Server();
s.listen(common.PIPE);
s.unref();

setTimeout(common.fail, 1000).unref();
