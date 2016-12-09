'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

// settings
var bytes = 1024 * 40;
var concurrency = 100;
var connections_per_client = 5;

// measured
var total_connections = 0;

var body = 'C'.repeat(bytes);

var server = net.createServer(function(c) {
  console.log('connected');
  total_connections++;
  console.log('#');
  c.write(body);
  c.end();
});

function runClient(callback) {
  var client = net.createConnection(common.PORT);

  client.connections = 0;

  client.setEncoding('utf8');

  client.on('connect', function() {
    console.log('c');
    client.recved = '';
    client.connections += 1;
  });

  client.on('data', function(chunk) {
    this.recved += chunk;
  });

  client.on('end', function() {
    client.end();
  });

  client.on('error', function(e) {
    console.log('\n\nERROOOOOr');
    throw e;
  });

  client.on('close', function(had_error) {
    console.log('.');
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
      if (++finished_clients === concurrency) server.close();
    });
  }
});

process.on('exit', function() {
  assert.equal(connections_per_client * concurrency, total_connections);
  console.log('\nokay!');
});
