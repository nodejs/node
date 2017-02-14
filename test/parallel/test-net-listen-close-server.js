'use strict';
const common = require('../common');
const net = require('net');

const server = net.createServer(function(socket) {
});
server.listen(0, common.mustNotCall());
server.on('error', common.mustNotCall());
server.close();
