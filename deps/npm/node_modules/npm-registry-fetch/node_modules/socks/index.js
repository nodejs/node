var SocksClient = require('./lib/socks-client.js');
var SocksAgent = require('./lib/socks-agent.js');

exports.createConnection = SocksClient.createConnection;
exports.createUDPFrame = SocksClient.createUDPFrame;
exports.Agent = SocksAgent.Agent;
