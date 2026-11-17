'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { once } = require('events');
const dc = require('diagnostics_channel');
const http2 = require('http2');

const kPublishCount = 8;

let currentSession;
let injection;

dc.subscribe('http2.client.request.headers', common.mustCall(({ headers, session }) => {
  assert.strictEqual(session, currentSession);
  assert.strictEqual(session.constructor.name, 'ClientHttp2Session');
  assert.ok(!!headers && typeof headers === 'object');
  if (injection !== undefined)
    injection(headers);
}, kPublishCount));

function doRequest(client, headers) {
  return new Promise((resolve, reject) => {
    const stream = client.request(headers);
    stream.setEncoding('utf8');
    let body = '';
    stream.on('data', (chunk) => { body += chunk; });
    stream.on('end', () => resolve(JSON.parse(body)));
    stream.on('error', reject);
  });
}

async function main() {
  const server = http2.createServer();
  server.on('stream', (stream, headers) => {
    // Echo the received request headers back in the response body.
    stream.respond({ 'content-type': 'application/json' });
    stream.end(JSON.stringify(headers));
  });
  server.listen(0);
  await once(server, 'listening');
  const port = server.address().port;

  // Scenario 1: request() called while the session is still connecting,
  // with an object headers argument.
  {
    const client = http2.connect(`http://localhost:${port}`);
    currentSession = client;
    injection = (headers) => {
      headers.traceparent = '00-0af7651916cd43dd8f8ba5c1d1f77f0f-00f067aa0ba902b7-01';
    };
    // Also make sure the snapshot published to 'http2.client.stream.created'
    // reflects the injected header, since serialization happens after the
    // 'http2.client.request.headers' publish.
    const onCreated = common.mustCall(({ headers }) => {
      assert.strictEqual(
        headers.traceparent,
        '00-0af7651916cd43dd8f8ba5c1d1f77f0f-00f067aa0ba902b7-01');
    }, 1);
    dc.subscribe('http2.client.stream.created', onCreated);
    const origHeaders = { ':path': '/connecting' };
    const received = await doRequest(client, origHeaders);
    dc.unsubscribe('http2.client.stream.created', onCreated);
    assert.strictEqual(
      received.traceparent,
      '00-0af7651916cd43dd8f8ba5c1d1f77f0f-00f067aa0ba902b7-01');
    assert.strictEqual(received[':path'], '/connecting');
    // The subscriber mutates a copy, so the caller's object is unchanged.
    assert.deepStrictEqual(origHeaders, { ':path': '/connecting' });
    client.close();
    await once(client, 'close');
  }

  // Scenario 2: request() called on an already-connected session.
  {
    const client = http2.connect(`http://localhost:${port}`);
    await once(client, 'connect');
    currentSession = client;
    injection = (headers) => {
      headers.traceparent = '00-0af7651916cd43dd8f8ba5c1d1f77f0f-b7ad6b7169203331-01';
    };
    const received = await doRequest(client, { ':path': '/connected' });
    assert.strictEqual(
      received.traceparent,
      '00-0af7651916cd43dd8f8ba5c1d1f77f0f-b7ad6b7169203331-01');
    assert.strictEqual(received[':path'], '/connected');
    client.close();
    await once(client, 'close');
  }

  // Scenario 3: no headers argument at all. The subscriber must still be
  // able to inject headers into the fresh object that is published.
  {
    const client = http2.connect(`http://localhost:${port}`);
    currentSession = client;
    let sawFreshObject;
    injection = (headers) => {
      sawFreshObject = headers[':path'] === undefined;
      headers.traceparent = '00-0af7651916cd43dd8f8ba5c1d1f77f0f-00f067aa0ba902b7-01';
    };
    const received = await doRequest(client);
    assert.ok(sawFreshObject);
    assert.strictEqual(
      received.traceparent,
      '00-0af7651916cd43dd8f8ba5c1d1f77f0f-00f067aa0ba902b7-01');
    assert.strictEqual(received[':path'], '/');
    client.close();
    await once(client, 'close');
  }

  // Scenario 4: a subscriber that does not mutate the headers must have no
  // effect on the request.
  {
    const client = http2.connect(`http://localhost:${port}`);
    await once(client, 'connect');
    currentSession = client;
    injection = undefined;
    const received = await doRequest(client, {
      ':path': '/no-mutation',
      'x-test': 'value',
    });
    assert.strictEqual(received.traceparent, undefined);
    assert.deepStrictEqual(received, {
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`,
      ':path': '/no-mutation',
      'x-test': 'value',
    });
    client.close();
    await once(client, 'close');
  }

  // Scenario 5: an array of raw headers can be mutated in place.
  {
    const client = http2.connect(`http://localhost:${port}`);
    currentSession = client;
    const origHeaders = [':path', '/array'];
    let sawArray;
    injection = (headers) => {
      sawArray = Array.isArray(headers);
      headers.push('traceparent', '00-0af7651916cd43dd8f8ba5c1d1f77f0f-00f067aa0ba902b7-01');
    };
    const received = await doRequest(client, origHeaders);
    assert.ok(sawArray);
    assert.strictEqual(
      received.traceparent,
      '00-0af7651916cd43dd8f8ba5c1d1f77f0f-00f067aa0ba902b7-01');
    assert.strictEqual(received[':path'], '/array');
    // The subscriber mutates a copy, so the caller's array is unchanged.
    assert.deepStrictEqual(origHeaders, [':path', '/array']);
    client.close();
    await once(client, 'close');
  }

  // Scenario 6: injected headers go through the same validation as headers
  // passed directly to request(). Injecting an unknown pseudo-header throws
  // the same error that passing it directly would.
  {
    const client = http2.connect(`http://localhost:${port}`);
    currentSession = client;
    injection = (headers) => {
      headers[':foo'] = 'invalid';
    };
    assert.throws(() => client.request({ ':path': '/invalid' }),
                  { code: 'ERR_HTTP2_INVALID_PSEUDOHEADER' });
    injection = undefined;
    client.destroy();
  }

  // Scenario 7: sensitiveHeaders is preserved on the published copy, in
  // object form, and mutating it does not touch the caller's list.
  {
    const client = http2.connect(`http://localhost:${port}`);
    currentSession = client;
    const origHeaders = {
      ':path': '/sensitive',
      'authorization': 'secret',
      [http2.sensitiveHeaders]: ['authorization'],
    };
    let sawSensitive;
    injection = (headers) => {
      sawSensitive = headers[http2.sensitiveHeaders]?.slice();
      headers[http2.sensitiveHeaders].push('traceparent');
      headers.traceparent = '00-0af7651916cd43dd8f8ba5c1d1f77f0f-00f067aa0ba902b7-01';
    };
    const received = await doRequest(client, origHeaders);
    assert.strictEqual(
      received.traceparent,
      '00-0af7651916cd43dd8f8ba5c1d1f77f0f-00f067aa0ba902b7-01');
    assert.deepStrictEqual(sawSensitive, ['authorization']);
    assert.deepStrictEqual(origHeaders, {
      ':path': '/sensitive',
      'authorization': 'secret',
      [http2.sensitiveHeaders]: ['authorization'],
    });
    client.close();
    await once(client, 'close');
  }

  // Scenario 8: same as scenario 7, in raw array form.
  {
    const client = http2.connect(`http://localhost:${port}`);
    currentSession = client;
    const origHeaders = [':path', '/sensitive-array', 'authorization', 'secret'];
    origHeaders[http2.sensitiveHeaders] = ['authorization'];
    let sawArray;
    let sawSensitive;
    injection = (headers) => {
      sawArray = Array.isArray(headers);
      sawSensitive = headers[http2.sensitiveHeaders]?.slice();
      headers[http2.sensitiveHeaders].push('traceparent');
      headers.push('traceparent', '00-0af7651916cd43dd8f8ba5c1d1f77f0f-00f067aa0ba902b7-01');
    };
    const received = await doRequest(client, origHeaders);
    assert.strictEqual(
      received.traceparent,
      '00-0af7651916cd43dd8f8ba5c1d1f77f0f-00f067aa0ba902b7-01');
    assert.ok(sawArray);
    assert.deepStrictEqual(sawSensitive, ['authorization']);
    assert.strictEqual(origHeaders.length, 4);
    assert.deepStrictEqual(origHeaders[http2.sensitiveHeaders], ['authorization']);
    client.close();
    await once(client, 'close');
  }

  server.close();
  await once(server, 'close');
}

main().then(common.mustCall());
