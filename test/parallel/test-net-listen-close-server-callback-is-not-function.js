'use strict';
const common = require('../common');
var net = require('net');

var server = net.createServer(common.fail);

server.on('close', common.mustCall(function() {}));

server.listen(0, common.fail);

server.close('bad argument');
