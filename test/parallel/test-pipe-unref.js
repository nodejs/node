'use strict';
const common = require('../common');
const net = require('net');

// This test should end immediately after `unref` is called

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const s = net.Server();
s.listen(common.PIPE);
s.unref();
