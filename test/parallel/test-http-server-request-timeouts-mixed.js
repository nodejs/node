'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { connect } = require('net');
const { promisify } = require('util');

// This test validates that request are correct checked for both requests and headers timeout in various situations.

const requestBodyPart1 = 'POST / HTTP/1.1\r\nContent-Length: 20\r\n';
const requestBodyPart2 = 'Connection: close\r\n\r\n1234567890';
const requestBodyPart3 = '1234567890';

const responseOk = 'HTTP/1.1 200 OK\r\n';
const responseTimeout = 'HTTP/1.1 408 Request Timeout\r\n';

const headersTimeout = common.platformTimeout(2000);
const connectionsCheckingInterval = headersTimeout / 4;
const requestTimeout = headersTimeout * 2;
const threadSleepDelay = requestTimeout + headersTimeout;
const delay = promisify(setTimeout);

const server = createServer({
  headersTimeout,
  requestTimeout,
  keepAliveTimeout: 0,
  connectionsCheckingInterval
}, common.mustCall((req, res) => {
  req.resume();

  req.on('end', () => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end();
  });
}, 4));

assert.strictEqual(server.headersTimeout, headersTimeout);
assert.strictEqual(server.requestTimeout, requestTimeout);

let i = 0;
function createClient(server) {
  const request = {
    index: i++,
    client: connect(server.address().port),
    response: '',
    completed: false
  };

  request.client.on('data', common.mustCallAtLeast((chunk) => {
    request.response += chunk.toString('utf-8');
  }));

  request.client.on('end', common.mustCall(() => {
    request.completed = true;
  }));

  request.client.on('error', common.mustNotCall());

  request.client.resume();

  return request;
}

server.listen(0, common.mustCall(async () => {
  // Create first and second request and set headers
  const request1 = createClient(server);
  const request2 = createClient(server);
  request1.client.write(requestBodyPart1);
  request2.client.write(requestBodyPart1);

  // Finish the first request now and wait more than the request timeout value
  request1.client.write(requestBodyPart2 + requestBodyPart3);
  await delay(threadSleepDelay);

  // First and second request should complete now
  assert(request1.completed);
  assert(request2.completed); // Completed, but a timeout request

  assert(request1.response.startsWith(responseOk));
  assert(request2.response.startsWith(responseTimeout)); // Reques expired due to headersTimeout

  // Create another 3 requests with headers
  const request3 = createClient(server);
  const request4 = createClient(server);
  const request5 = createClient(server);
  request3.client.write(requestBodyPart1 + requestBodyPart2);
  request4.client.write(requestBodyPart1 + requestBodyPart2);
  request5.client.write(requestBodyPart1 + requestBodyPart2);

  // None of the requests will be completed because they only recieved headers
  assert(!request3.completed);
  assert(!request4.completed);
  assert(!request5.completed);

  // Finish the fourth request now and wait more than the request timeout value
  request4.client.write(requestBodyPart3);
  await delay(threadSleepDelay);

  // All three requests should complete now
  assert(request3.completed);
  assert(request4.completed);
  assert(request5.completed);

  assert(request3.response.startsWith(responseTimeout)); // It is expired due to headersTimeout
  assert(request4.response.startsWith(responseOk));
  assert(request5.response.startsWith(responseTimeout)); // It is expired due to headersTimeout
  server.close();
}));
