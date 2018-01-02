'use strict';
const common = require('../common');
const net = require('net');

// This test should end immediately after `unref` is called

common.refreshTmpDir();

const s = net.Server();
s.listen(common.PIPE);
s.unref();
