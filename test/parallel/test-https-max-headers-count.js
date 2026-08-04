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

const headers = {
  host: 'example.com'
};
const N = 100;
for (let i = 0; i < N; ++i) {
  headers[`key${i}`] = i;
}

function createRequestHeaders(count) {
  const requestHeaders = {
    host: 'example.com',
  };
  for (let i = 0; i < count; ++i) {
    requestHeaders[`key${i}`] = i;
  }
  return requestHeaders;
}

const serverMaxAndExpected = [ // for server
  [50, 50, 48],
  [1500, 102, N],
  [0, N + 2, N], // Host and Connection
];
let max = serverMaxAndExpected[requests][0];
let expected = serverMaxAndExpected[requests][1];

const server = https.createServer(serverOptions, common.mustCall((req, res) => {
  assert.strictEqual(Object.keys(req.headers).length, expected);
  if (++requests < serverMaxAndExpected.length) {
    max = serverMaxAndExpected[requests][0];
    expected = serverMaxAndExpected[requests][1];
    server.maxHeadersCount = max;
  }
  res.writeHead(200, { ...headers, 'Connection': 'close' });
  res.end();
}, 3));
server.maxHeadersCount = max;

server.listen(0, common.mustCall(() => {
  const clientMaxAndExpected = [ // for client
    [20, 20],
    [1200, 104],
    [0, N + 4], // Host and Connection
  ];
  const doRequest = common.mustCall(() => {
    const max = clientMaxAndExpected[responses][0];
    const expected = clientMaxAndExpected[responses][1];
    const requestHeaders =
      createRequestHeaders(serverMaxAndExpected[requests][2]);
    const req = https.request({
      port: server.address().port,
      headers: requestHeaders,
      rejectUnauthorized: false
    }, common.mustCall((res) => {
      assert.strictEqual(Object.keys(res.headers).length, expected);
      res.on('end', () => {
        if (++responses < clientMaxAndExpected.length) {
          doRequest();
        } else {
          server.close();
        }
      });
      res.resume();
    }));
    req.maxHeadersCount = max;
    req.end();
  }, 3);
  doRequest();
}));
