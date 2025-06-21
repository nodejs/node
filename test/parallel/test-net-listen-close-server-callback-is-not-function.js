'use strict';
const common = require('../common');
const net = require('net');

const server = net.createServer(common.mustNotCall());

server.on('close', common.mustCall());

server.listen(0, common.mustNotCall());

server.close('bad argument');
