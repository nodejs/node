'use strict';
// Tests of multiple domains happening at once.

require('../common');
var assert = require('assert');
var domain = require('domain');

var caughtA = false;
var caughtB = false;
var caughtC = false;


var a = domain.create();
a.enter(); // this will be our "root" domain
a.on('error', function(er) {
  caughtA = true;
  console.log('This should not happen');
  throw er;
});


var http = require('http');
var server = http.createServer(function(req, res) {
  // child domain of a.
  var b = domain.create();
  a.add(b);

  // treat these EE objects as if they are a part of the b domain
  // so, an 'error' event on them propagates to the domain, rather
  // than being thrown.
  b.add(req);
  b.add(res);

  b.on('error', function(er) {
    caughtB = true;
    console.error('Error encountered', er);
    if (res) {
      res.writeHead(500);
      res.end('An error occurred');
    }
    // res.writeHead(500), res.destroy, etc.
    server.close();
  });

  // XXX this bind should not be necessary.
  // the write cb behavior in http/net should use an
  // event so that it picks up the domain handling.
  res.write('HELLO\n', b.bind(function() {
    throw new Error('this kills domain B, not A');
  }));

}).listen(0, function() {
  var c = domain.create();
  var req = http.get({ host: 'localhost', port: this.address().port });

  // add the request to the C domain
  c.add(req);

  req.on('response', function(res) {
    console.error('got response');
    // add the response object to the C domain
    c.add(res);
    res.pipe(process.stdout);
  });

  c.on('error', function(er) {
    caughtC = true;
    console.error('Error on c', er.message);
  });
});

process.on('exit', function() {
  assert.equal(caughtA, false);
  assert.equal(caughtB, true);
  assert.equal(caughtC, true);
  console.log('ok - Errors went where they were supposed to go');
});
