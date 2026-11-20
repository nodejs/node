'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const body =
  '<html><head></head><body><h1>this is some data</h2></body></html>';

// Starts a server with the given request handler, sends one request and
// calls onResponse with the response headers.
function testResponse(onRequest, onResponse) {
  const server = http2.createServer(onRequest);
  server.on('error', common.mustNotCall());

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request({ ':path': '/' });
    req.setEncoding('utf8');
    req.on('response', onResponse);

    let data = '';
    req.on('data', (d) => data += d);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(data, body);
      server.close();
      client.close();
    }));
    req.end();
  }));
}

// Map, including a header name that is not lower case and a connection
// header, which is dropped with a warning.
{
  const compatMsg = 'The provided connection header is not valid, ' +
                    'the value will be dropped from the header and ' +
                    'will never be in use.';
  common.expectWarning('UnsupportedWarning', compatMsg);

  testResponse(common.mustCall((req, res) => {
    res.setHeaders(new Map([
      ['foobar', 'baz'],
      ['X-POWERED-BY', 'node-test'],
      ['connection', 'connection-test'],
    ]));
    res.end(body);
  }), common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 200);
    assert.strictEqual(headers.foobar, 'baz');
    assert.strictEqual(headers['x-powered-by'], 'node-test');
    assert.strictEqual(headers.connection, undefined);
  }));
}

// Headers instance.
{
  testResponse(common.mustCall((req, res) => {
    res.setHeaders(new Headers({ foo: '1', bar: '2' }));
    res.writeHead(200);
    res.end(body);
  }), common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 200);
    assert.strictEqual(headers.foo, '1');
    assert.strictEqual(headers.bar, '2');
  }));
}

// An existing header is replaced.
{
  testResponse(common.mustCall((req, res) => {
    res.setHeader('foo', '1');
    res.setHeader('bar', '2');
    res.setHeaders(new Headers({ foo: '3' }));
    res.end(body);
  }), common.mustCall((headers) => {
    assert.strictEqual(headers.foo, '3');
    assert.strictEqual(headers.bar, '2');
  }));
}

// Headers passed to writeHead() as an object take precedence.
{
  testResponse(common.mustCall((req, res) => {
    res.setHeaders(new Headers({ foo: '1', bar: '2' }));
    res.writeHead(200, { foo: '3' });
    res.end(body);
  }), common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 200);
    assert.strictEqual(headers.foo, '3');
    assert.strictEqual(headers.bar, '2');
  }));
}

// Headers passed to writeHead() as an array take precedence.
{
  testResponse(common.mustCall((req, res) => {
    res.setHeaders(new Headers({ foo: '1', bar: '2' }));
    res.writeHead(200, ['foo', '3']);
    res.end(body);
  }), common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 200);
    assert.strictEqual(headers.foo, '3');
    assert.strictEqual(headers.bar, '2');
  }));
}

// Multiple set-cookie values appended to a Headers instance.
{
  testResponse(common.mustCall((req, res) => {
    const headers = new Headers();
    headers.append('Set-Cookie', 'a=b');
    headers.append('Set-Cookie', 'c=d');
    res.setHeaders(headers);
    res.end(body);
  }), common.mustCall((headers) => {
    assert.deepStrictEqual(headers['set-cookie'], ['a=b', 'c=d']);
  }));
}

// Multiple set-cookie values as an array in a Map, with a name that is not
// lower case.
{
  testResponse(common.mustCall((req, res) => {
    res.setHeaders(new Map([['Set-Cookie', ['a=b', 'c=d']]]));
    res.end(body);
  }), common.mustCall((headers) => {
    assert.deepStrictEqual(headers['set-cookie'], ['a=b', 'c=d']);
  }));
}

// Lower case set-cookie in a Map, as an array and as a single value.
{
  testResponse(common.mustCall((req, res) => {
    res.setHeaders(new Map([['set-cookie', ['a=b', 'c=d']]]));
    res.end(body);
  }), common.mustCall((headers) => {
    assert.deepStrictEqual(headers['set-cookie'], ['a=b', 'c=d']);
  }));

  testResponse(common.mustCall((req, res) => {
    res.setHeaders(new Map([['set-cookie', 'a=b']]));
    res.end(body);
  }), common.mustCall((headers) => {
    assert.deepStrictEqual(headers['set-cookie'], ['a=b']);
  }));
}

// set-cookie values replace the ones set before.
{
  testResponse(common.mustCall((req, res) => {
    res.setHeader('set-cookie', ['x=y']);
    const headers = new Headers();
    headers.append('Set-Cookie', 'a=b');
    headers.append('Set-Cookie', 'c=d');
    res.setHeaders(headers);
    res.end(body);
  }), common.mustCall((headers) => {
    assert.deepStrictEqual(headers['set-cookie'], ['a=b', 'c=d']);
  }));
}

// Throws when the headers have already been sent.
{
  testResponse(common.mustCall((req, res) => {
    res.writeHead(200);
    assert.throws(() => {
      res.setHeaders(new Headers({ foo: '1' }));
    }, {
      code: 'ERR_HTTP2_HEADERS_SENT',
      name: 'Error',
    });
    res.end(body);
  }), common.mustCall((headers) => {
    assert.strictEqual(headers.foo, undefined);
  }));
}

// Throws when headers is neither a Headers instance nor a Map.
{
  testResponse(common.mustCall((req, res) => {
    for (const headers of [
      ['foo', '1'],
      { foo: '1' },
      null,
      undefined,
      'test',
      1,
      { keys() {} },
      { get() {} },
    ]) {
      assert.throws(() => {
        res.setHeaders(headers);
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      });
    }
    res.end(body);
  }), common.mustCall((headers) => {
    assert.strictEqual(headers.foo, undefined);
  }));
}
