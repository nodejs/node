'use strict';
const common = require('../common');
const net = require('net');

common.refreshTmpDir();

const s = net.Server();
s.listen(common.PIPE);
s.unref();

setTimeout(common.mustNotCall(), 1000).unref();
