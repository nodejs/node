var assert = require('assert');
var common = require('../common');
var fork = require('child_process').fork;
var net = require('net');

var n = fork(common.fixturesDir + '/fork2.js');

var messageCount = 0;

var server = new net.Server();
server.listen(common.PORT, function() {
  console.log('PARENT send child server handle');
  n.send({ hello: 'world' }, server);
});


n.on('message', function(m) {
  console.log('PARENT got message:', m);
  assert.ok(m.gotHandle);
  messageCount++;
  n.kill();
  server.close();
});

process.on('exit', function() {
  assert.equal(1, messageCount);
});
