'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');

const serverOptions = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

let requests = 0;
let responses = 0;

const headers = {};
const N = 100;
for (let i = 0; i < N; ++i) {
  headers[`key${i}`] = i;
}

const maxAndExpected = [ // for server
  [50, 50],
  [1500, 102],
  [0, N + 2] // Host and Connection
];
let max = maxAndExpected[requests][0];
let expected = maxAndExpected[requests][1];

const server = https.createServer(serverOptions, common.mustCall((req, res) => {
  assert.strictEqual(Object.keys(req.headers).length, expected);
  if (++requests < maxAndExpected.length) {
    max = maxAndExpected[requests][0];
    expected = maxAndExpected[requests][1];
    server.maxHeadersCount = max;
  }
  res.writeHead(200, headers);
  res.end();
}, 3));
server.maxHeadersCount = max;

server.listen(0, common.mustCall(() => {
  const maxAndExpected = [ // for client
    [20, 20],
    [1200, 103],
    [0, N + 3] // Connection, Date and Transfer-Encoding
  ];
  const doRequest = common.mustCall(() => {
    const max = maxAndExpected[responses][0];
    const expected = maxAndExpected[responses][1];
    const req = https.request({
      port: server.address().port,
      headers: headers,
      rejectUnauthorized: false
    }, (res) => {
      assert.strictEqual(Object.keys(res.headers).length, expected);
      res.on('end', () => {
        if (++responses < maxAndExpected.length) {
          doRequest();
        } else {
          server.close();
        }
      });
      res.resume();
    });
    req.maxHeadersCount = max;
    req.end();
  }, 3);
  doRequest();
}));
