'use strict';
require('../common');
var assert = require('assert');

var http = require('http');


var serverSockets = [];
var server = http.createServer(function(req, res) {
  if (serverSockets.indexOf(req.socket) === -1) {
    serverSockets.push(req.socket);
  }
  res.end(req.url);
});
server.listen(0, function() {
  var agent = http.Agent({
    keepAlive: true,
    maxSockets: 5,
    maxFreeSockets: 2
  });

  var closed = false;
  makeReqs(10, function(er) {
    assert.ifError(er);
    assert.equal(count(agent.freeSockets), 2);
    assert.equal(count(agent.sockets), 0);
    assert.equal(serverSockets.length, 5);

    // now make 10 more reqs.
    // should use the 2 free reqs from the pool first.
    makeReqs(10, function(er) {
      assert.ifError(er);
      assert.equal(count(agent.freeSockets), 2);
      assert.equal(count(agent.sockets), 0);
      assert.equal(serverSockets.length, 8);

      agent.destroy();
      server.close(function() {
        closed = true;
      });
    });
  });

  process.on('exit', function() {
    assert(closed);
    console.log('ok');
  });

  // make 10 requests in parallel,
  // then 10 more when they all finish.
  function makeReqs(n, cb) {
    for (var i = 0; i < n; i++)
      makeReq(i, then);

    function then(er) {
      if (er)
        return cb(er);
      else if (--n === 0)
        setTimeout(cb, 100);
    }
  }

  function makeReq(i, cb) {
    http.request({
      port: server.address().port,
      path: '/' + i,
      agent: agent
    }, function(res) {
      var data = '';
      res.setEncoding('ascii');
      res.on('data', function(c) {
        data += c;
      });
      res.on('end', function() {
        assert.equal(data, '/' + i);
        cb();
      });
    }).end();
  }

  function count(sockets) {
    return Object.keys(sockets).reduce(function(n, name) {
      return n + sockets[name].length;
    }, 0);
  }
});
