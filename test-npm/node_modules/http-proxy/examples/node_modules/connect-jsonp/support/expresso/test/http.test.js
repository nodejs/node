
/**
 * Module dependencies.
 */

var assert = require('assert')
  , http = require('http');

var server = http.createServer(function(req, res){
  if (req.method === 'GET') {
    if (req.url === '/delay') {
      setTimeout(function(){
        res.writeHead(200, {});
        res.end('delayed');
      }, 200);
    } else {
      var body = JSON.stringify({ name: 'tj' });
      res.writeHead(200, {
        'Content-Type': 'application/json; charset=utf8',
        'Content-Length': body.length
      });
      res.end(body);
    }
  } else {
    var body = '';
    req.setEncoding('utf8');
    req.on('data', function(chunk){ body += chunk });
    req.on('end', function(){
      res.writeHead(200, {});
      res.end(req.url + ' ' + body);
    });
  }
});

var delayedServer = http.createServer(function(req, res){
  res.writeHead(200);
  res.end('it worked');
});

var oldListen = delayedServer.listen;
delayedServer.listen = function(){
  var args = arguments;
  setTimeout(function(){
    oldListen.apply(delayedServer, args);
  }, 100);
};

module.exports = {
  'test assert.response(req, res, fn)': function(beforeExit){
    var calls = 0;

    assert.response(server, {
      url: '/',
      method: 'GET'
    },{
      body: '{"name":"tj"}',
      status: 200,
      headers: {
        'Content-Type': 'application/json; charset=utf8'
      }
    }, function(res){
      ++calls;
      assert.ok(res);
    });
    
    beforeExit(function(){
      assert.equal(1, calls);
    })
  },

  'test assert.response(req, fn)': function(beforeExit){
    var calls = 0;

    assert.response(server, {
      url: '/foo'
    }, function(res){
      ++calls;
      assert.ok(res.body.indexOf('tj') >= 0, 'Test assert.response() callback');
    });

    beforeExit(function(){
      assert.equal(1, calls);
    });
  },
  
  'test assert.response() delay': function(beforeExit){
    var calls = 0;

    assert.response(server,
      { url: '/delay', timeout: 1500 },
      { body: 'delayed' },
      function(){
        ++calls;
      });

    beforeExit(function(){
      assert.equal(1, calls);
    });
  },

  'test assert.response() regexp': function(beforeExit){
    var calls = 0;

    assert.response(server,
      { url: '/foo', method: 'POST', data: 'foobar' },
      { body: /^\/foo foo(bar)?/ },
      function(){
        ++calls;
      });
    
    beforeExit(function(){
      assert.equal(1, calls);
    });
  },
  
  'test assert.response() regexp headers': function(beforeExit){
    var calls = 0;

    assert.response(server,
      { url: '/' },
      { body: '{"name":"tj"}', headers: { 'Content-Type': /^application\/json/ } },
      function(){
        ++calls;
      });
    
    beforeExit(function(){
      assert.equal(1, calls);
    });
  },

  // [!] if this test doesn't pass, an uncaught ECONNREFUSED will display
  'test assert.response() with deferred listen()': function(beforeExit){
    var calls = 0;

    assert.response(delayedServer,
      { url: '/' },
      { body: 'it worked' },
      function(){
        ++calls;
      });

    beforeExit(function(){
      assert.equal(1, calls);
    });
  }
};
