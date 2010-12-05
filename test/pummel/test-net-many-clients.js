var common = require('../common');
var assert = require('assert');
var net = require('net');

// settings
var bytes = 1024 * 40;
var concurrency = 100;
var connections_per_client = 5;

// measured
var total_connections = 0;

var body = '';
for (var i = 0; i < bytes; i++) {
  body += 'C';
}

var server = net.createServer(function(c) {
  c.addListener('connect', function() {
    total_connections++;
    common.print('#');
    c.write(body);
    c.end();
  });
});

function runClient(callback) {
  var client = net.createConnection(common.PORT);

  client.connections = 0;

  client.setEncoding('utf8');

  client.addListener('connect', function() {
    common.print('c');
    client.recved = '';
    client.connections += 1;
  });

  client.addListener('data', function(chunk) {
    this.recved += chunk;
  });

  client.addListener('end', function() {
    client.end();
  });

  client.addListener('error', function(e) {
    console.log('\n\nERROOOOOr');
    throw e;
  });

  client.addListener('close', function(had_error) {
    common.print('.');
    assert.equal(false, had_error);
    assert.equal(bytes, client.recved.length);

    if (client.fd) {
      console.log(client.fd);
    }
    assert.ok(!client.fd);

    if (this.connections < connections_per_client) {
      this.connect(common.PORT);
    } else {
      callback();
    }
  });
}

server.listen(common.PORT, function() {
  var finished_clients = 0;
  for (var i = 0; i < concurrency; i++) {
    runClient(function() {
      if (++finished_clients == concurrency) server.close();
    });
  }
});

process.addListener('exit', function() {
  assert.equal(connections_per_client * concurrency, total_connections);
  console.log('\nokay!');
});
