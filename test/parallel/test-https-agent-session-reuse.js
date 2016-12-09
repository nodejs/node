'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const https = require('https');
const crypto = require('crypto');

const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

const ca = fs.readFileSync(common.fixturesDir + '/keys/ca1-cert.pem');

const clientSessions = {};
let serverRequests = 0;

const agent = new https.Agent({
  maxCachedSessions: 1
});

const server = https.createServer(options, function(req, res) {
  if (req.url === '/drop-key')
    server.setTicketKeys(crypto.randomBytes(48));

  serverRequests++;
  res.end('ok');
}).listen(0, function() {
  const queue = [
    {
      name: 'first',

      method: 'GET',
      path: '/',
      servername: 'agent1',
      ca: ca,
      port: this.address().port
    },
    {
      name: 'first-reuse',

      method: 'GET',
      path: '/',
      servername: 'agent1',
      ca: ca,
      port: this.address().port
    },
    {
      name: 'cipher-change',

      method: 'GET',
      path: '/',
      servername: 'agent1',

      // Choose different cipher to use different cache entry
      ciphers: 'AES256-SHA',
      ca: ca,
      port: this.address().port
    },
    // Change the ticket key to ensure session is updated in cache
    {
      name: 'before-drop',

      method: 'GET',
      path: '/drop-key',
      servername: 'agent1',
      ca: ca,
      port: this.address().port
    },

    // Ticket will be updated starting from this
    {
      name: 'after-drop',

      method: 'GET',
      path: '/',
      servername: 'agent1',
      ca: ca,
      port: this.address().port
    },
    {
      name: 'after-drop-reuse',

      method: 'GET',
      path: '/',
      servername: 'agent1',
      ca: ca,
      port: this.address().port
    }
  ];

  function request() {
    const options = queue.shift();
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
  assert.strictEqual(serverRequests, 6);
  assert.strictEqual(clientSessions['first'].toString('hex'),
                     clientSessions['first-reuse'].toString('hex'));
  assert.notEqual(clientSessions['first'].toString('hex'),
                  clientSessions['cipher-change'].toString('hex'));
  assert.notEqual(clientSessions['first'].toString('hex'),
                  clientSessions['before-drop'].toString('hex'));
  assert.notEqual(clientSessions['cipher-change'].toString('hex'),
                  clientSessions['before-drop'].toString('hex'));
  assert.notEqual(clientSessions['before-drop'].toString('hex'),
                  clientSessions['after-drop'].toString('hex'));
  assert.strictEqual(clientSessions['after-drop'].toString('hex'),
                     clientSessions['after-drop-reuse'].toString('hex'));
});
