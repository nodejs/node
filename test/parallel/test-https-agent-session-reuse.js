'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}

var https = require('https');
var crypto = require('crypto');

var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var ca = fs.readFileSync(common.fixturesDir + '/keys/ca1-cert.pem');

var clientSessions = {};
var serverRequests = 0;

var agent = new https.Agent({
  maxCachedSessions: 1
});

var server = https.createServer(options, function(req, res) {
  if (req.url === '/drop-key')
    server.setTicketKeys(crypto.randomBytes(48));

  serverRequests++;
  res.end('ok');
}).listen(common.PORT, function() {
  var queue = [
    {
      name: 'first',

      method: 'GET',
      path: '/',
      servername: 'agent1',
      ca: ca,
      port: common.PORT
    },
    {
      name: 'first-reuse',

      method: 'GET',
      path: '/',
      servername: 'agent1',
      ca: ca,
      port: common.PORT
    },
    {
      name: 'cipher-change',

      method: 'GET',
      path: '/',
      servername: 'agent1',

      // Choose different cipher to use different cache entry
      ciphers: 'AES256-SHA',
      ca: ca,
      port: common.PORT
    },
    // Change the ticket key to ensure session is updated in cache
    {
      name: 'before-drop',

      method: 'GET',
      path: '/drop-key',
      servername: 'agent1',
      ca: ca,
      port: common.PORT
    },

    // Ticket will be updated starting from this
    {
      name: 'after-drop',

      method: 'GET',
      path: '/',
      servername: 'agent1',
      ca: ca,
      port: common.PORT
    },
    {
      name: 'after-drop-reuse',

      method: 'GET',
      path: '/',
      servername: 'agent1',
      ca: ca,
      port: common.PORT
    }
  ];

  function request() {
    var options = queue.shift();
    options.agent = agent;
    https.request(options, function(res) {
      clientSessions[options.name] = res.socket.getSession();

      res.resume();
      res.on('end', function() {
        if (queue.length !== 0)
          return request();
        server.close();
      });
    }).end();
  }
  request();
});

process.on('exit', function() {
  assert.equal(serverRequests, 6);
  assert.equal(clientSessions['first'].toString('hex'),
               clientSessions['first-reuse'].toString('hex'));
  assert.notEqual(clientSessions['first'].toString('hex'),
                  clientSessions['cipher-change'].toString('hex'));
  assert.notEqual(clientSessions['first'].toString('hex'),
                  clientSessions['before-drop'].toString('hex'));
  assert.notEqual(clientSessions['cipher-change'].toString('hex'),
                  clientSessions['before-drop'].toString('hex'));
  assert.notEqual(clientSessions['before-drop'].toString('hex'),
                  clientSessions['after-drop'].toString('hex'));
  assert.equal(clientSessions['after-drop'].toString('hex'),
               clientSessions['after-drop-reuse'].toString('hex'));
});
