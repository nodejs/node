var net = require('net');
var sys = require('sys');

var warning;
if (!warning) {
  warning = "The 'tcp' module is now called 'net'. Otherwise it should have a similar interface.";
  sys.error(warning);
}

exports.createServer = net.createServer;
exports.createConnection = net.createConnection;
