'use strict';
const common = require('../common');
var net = require('net');

var server = net.createServer(function(socket) {
});
server.listen(0, common.fail);
server.on('error', common.fail);
server.close();
