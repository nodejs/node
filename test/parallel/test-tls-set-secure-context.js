'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const fixtures = require('../common/fixtures');
const credentialOptions = [
  {
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
    ca: fixtures.readKey('ca1-cert.pem')
  },
  {
    key: fixtures.readKey('agent2-key.pem'),
    cert: fixtures.readKey('agent2-cert.pem'),
    ca: fixtures.readKey('ca2-cert.pem')
  }
];
let requestsCount = 0;
let firstResponse;

const server = https.createServer(credentialOptions[0], (req, res) => {
  requestsCount++;

  if (requestsCount === 1) {
    firstResponse = res;
    firstResponse.write('multi-');
    return;
  } else if (requestsCount === 3) {
    firstResponse.write('success-');
  }

  res.end('success');
});

server.listen(0, common.mustCall(async () => {
  const { port } = server.address();
  const firstRequest = makeRequest(port);

  assert.strictEqual(await makeRequest(port), 'success');

  server.setSecureContext(credentialOptions[1]);
  firstResponse.write('request-');
  await assert.rejects(async () => {
    await makeRequest(port);
  }, /^Error: self signed certificate$/);

  server.setSecureContext(credentialOptions[0]);
  assert.strictEqual(await makeRequest(port), 'success');

  server.setSecureContext(credentialOptions[1]);
  firstResponse.end('fun!');
  await assert.rejects(async () => {
    await makeRequest(port);
  }, /^Error: self signed certificate$/);

  assert.strictEqual(await firstRequest, 'multi-request-success-fun!');
  server.close();
}));

function makeRequest(port) {
  return new Promise((resolve, reject) => {
    const options = {
      rejectUnauthorized: true,
      ca: credentialOptions[0].ca,
      servername: 'agent1'
    };

    https.get(`https://localhost:${port}`, options, (res) => {
      let response = '';

      res.setEncoding('utf8');

      res.on('data', (chunk) => {
        response += chunk;
      });

      res.on('end', common.mustCall(() => {
        resolve(response);
      }));
    }).on('error', (err) => {
      reject(err);
    });
  });
}
