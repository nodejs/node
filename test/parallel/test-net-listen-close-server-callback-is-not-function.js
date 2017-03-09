'use strict';
const common = require('../common');
const net = require('net');

const server = net.createServer(common.fail);

server.on('close', common.mustCall(function() {}));

server.listen(0, common.fail);

server.close('bad argument');
