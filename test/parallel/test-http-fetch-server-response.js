// Flags: --experimental-http-serve --no-warnings
'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { ServerResponse } = require('http').fetchServer;

async function text(iterable) {
  const decoder = new TextDecoder();
  let result = '';
  for await (const chunks of iterable) {
    for (const chunk of chunks) {
      assert.ok(chunk instanceof Uint8Array);
      result += decoder.decode(chunk, { stream: true });
    }
  }
  return result + decoder.decode();
}

test('fetchServer requires the flag', () => {
  const { spawnSync } = require('child_process');
  const child = spawnSync(process.execPath, [
    '-p', 'require("http").fetchServer === undefined',
  ]);
  assert.strictEqual(child.stdout.toString().trim(), 'true');
});

test('defaults', async () => {
  const response = new ServerResponse();
  assert.strictEqual(response.type, 'default');
  assert.strictEqual(response.status, 200);
  assert.strictEqual(response.ok, true);
  assert.strictEqual(response.statusText, '');
  assert.strictEqual(response.url, '');
  assert.strictEqual(response.redirected, false);
  assert.strictEqual(response.headers, response.headers);
  assert.strictEqual(await text(response), '');
  const trailers = await response.trailers;
  assert.deepStrictEqual([...trailers], []);
  assert.strictEqual(response.trailers, response.trailers);
  assert.strictEqual(Object.prototype.toString.call(response),
                     '[object ServerResponse]');
});

test('string body and content type', async () => {
  const response = new ServerResponse('hello', {
    status: 201,
    statusText: 'Created',
    headers: [['x-a', '1'], ['X-A', '2']],
  });
  assert.strictEqual(response.status, 201);
  assert.strictEqual(response.statusText, 'Created');
  assert.strictEqual(response.headers.get('x-a'), '1, 2');
  assert.strictEqual(response.headers.get('content-type'),
                     'text/plain;charset=UTF-8');
  assert.strictEqual(await text(response), 'hello');
});

test('body types', async () => {
  assert.strictEqual(await text(new ServerResponse(new Uint8Array([104, 105]))), 'hi');
  assert.strictEqual(await text(new ServerResponse(new Uint8Array([104, 105]).buffer)), 'hi');
  assert.strictEqual(await text(new ServerResponse(new Blob(['blob']))), 'blob');
  const params = new ServerResponse(new URLSearchParams({ a: 'b c' }));
  assert.strictEqual(params.headers.get('content-type'),
                     'application/x-www-form-urlencoded;charset=UTF-8');
  assert.strictEqual(await text(params), 'a=b+c');
  const rs = new ReadableStream({
    start(c) {
      c.enqueue(new TextEncoder().encode('rs'));
      c.close();
    },
  });
  assert.strictEqual(await text(new ServerResponse(rs)), 'rs');
  async function* gen() {
    yield [new TextEncoder().encode('a'), new TextEncoder().encode('b')];
    yield [];
    yield [new TextEncoder().encode('c')];
  }
  assert.strictEqual(await text(new ServerResponse(gen())), 'abc');
  // A sync iterable of batches is accepted as an async sequence.
  assert.strictEqual(await text(new ServerResponse([[new Uint8Array([120])]])), 'x');
  // A ServerResponse can be the body of another.
  assert.strictEqual(await text(new ServerResponse(new ServerResponse('nested'))),
                     'nested');
  const form = new FormData();
  form.append('a', 'b');
  const formResponse = new ServerResponse(form);
  assert.match(formResponse.headers.get('content-type'),
               /^multipart\/form-data; boundary=/);
  assert.match(await text(formResponse), /name="a"/);
});

test('yielding a bare chunk fails when read', async () => {
  async function* gen() {
    yield new Uint8Array([1]);
  }
  await assert.rejects(text(new ServerResponse(gen())), { name: 'TypeError' });
});

test('validation', () => {
  assert.throws(() => new ServerResponse(null, { status: 199 }), RangeError);
  assert.throws(() => new ServerResponse(null, { status: 600 }), RangeError);
  assert.throws(() => new ServerResponse(null, { status: 70000 }), TypeError);
  assert.throws(() => new ServerResponse('x', { status: 204 }), TypeError);
  assert.throws(() => new ServerResponse(null, { statusText: 'a\nb' }), TypeError);
  assert.throws(() => new ServerResponse(null, { headers: { 'a b': 'c' } }),
                TypeError);
  assert.throws(() => new ServerResponse(null, { headers: { a: 'b\r\nc' } }),
                TypeError);
  assert.throws(() => new ServerResponse(null, { headers: [['a']] }), TypeError);
  const locked = new ReadableStream();
  locked.getReader();
  assert.throws(() => new ServerResponse(locked), TypeError);
});

test('static methods', async () => {
  const error = ServerResponse.error();
  assert.strictEqual(error.type, 'error');
  assert.strictEqual(error.status, 0);
  error.headers.set('x', 'y');

  const redirect = ServerResponse.redirect('/relative?a=b', 307);
  assert.strictEqual(redirect.status, 307);
  assert.strictEqual(redirect.headers.get('location'), '/relative?a=b');
  assert.strictEqual(ServerResponse.redirect('/x').status, 302);
  assert.throws(() => ServerResponse.redirect('/a b'), TypeError);
  assert.throws(() => ServerResponse.redirect('/', 200), RangeError);

  const json = ServerResponse.json({ a: 1 }, { status: 202 });
  assert.strictEqual(json.status, 202);
  assert.strictEqual(json.headers.get('content-type'), 'application/json');
  assert.strictEqual(await text(json), '{"a":1}');
  assert.throws(() => ServerResponse.json(undefined), TypeError);

  const from = ServerResponse.from(new Response('upstream', {
    status: 404,
    statusText: 'Nope',
    headers: { 'x-up': '1' },
  }));
  assert.strictEqual(from.status, 404);
  assert.strictEqual(from.statusText, 'Nope');
  assert.strictEqual(from.headers.get('x-up'), '1');
  assert.strictEqual(await text(from), 'upstream');
  assert.strictEqual(ServerResponse.from(Response.error()).type, 'error');
  const used = new Response('x');
  await used.text();
  assert.throws(() => ServerResponse.from(used), TypeError);
});

test('body is read once', async () => {
  const response = new ServerResponse('once');
  const first = response[Symbol.asyncIterator]();
  const second = response[Symbol.asyncIterator]();
  await assert.rejects(second.next(), TypeError);
  assert.strictEqual((await first.next()).done, false);
  assert.throws(() => response.clone(), TypeError);
});

test('clone splits the body', async () => {
  const response = new ServerResponse('clone me', { headers: { a: 'b' } });
  const clone = response.clone();
  assert.strictEqual(clone.headers.get('a'), 'b');
  assert.notStrictEqual(clone.headers, response.headers);
  const [a, b] = await Promise.all([text(response), text(clone)]);
  assert.strictEqual(a, 'clone me');
  assert.strictEqual(b, 'clone me');
});

test('trailers', async () => {
  const response = new ServerResponse('x', {
    headers: { Trailer: 'x-count, x-other' },
    trailers: Promise.resolve({ 'x-count': '1', 'x-undeclared': '2' }),
  });
  const trailers = await response.trailers;
  assert.deepStrictEqual([...trailers], [['x-count', '1']]);
  assert.throws(() => trailers.set('a', 'b'), { code: 'ERR_HTTP_HEADERS_SENT' });
  const clone = response.clone();
  assert.notStrictEqual(clone.trailers, response.trailers);
  assert.deepStrictEqual([...await clone.trailers], [['x-count', '1']]);

  const failing = new ServerResponse(null, {
    trailers: Promise.reject(new Error('nope')),
  });
  await assert.rejects(failing.trailers, /nope/);
});

test('headers behave like Headers without guards', () => {
  const { headers } = new ServerResponse();
  headers.append('Set-Cookie', 'a=1');
  headers.append('set-cookie', 'b=2');
  headers.set('X-B', ' padded ');
  headers.append('x-a', '1');
  assert.deepStrictEqual(headers.getSetCookie(), ['a=1', 'b=2']);
  assert.strictEqual(headers.get('x-b'), 'padded');
  assert.deepStrictEqual([...headers], [
    ['set-cookie', 'a=1'],
    ['set-cookie', 'b=2'],
    ['x-a', '1'],
    ['x-b', 'padded'],
  ]);
  assert.deepStrictEqual([...headers.keys()],
                         ['set-cookie', 'set-cookie', 'x-a', 'x-b']);
  assert.strictEqual(headers[Symbol.iterator], headers.entries);
  headers.delete('X-A');
  assert.strictEqual(headers.has('x-a'), false);
  const seen = [];
  headers.forEach((value, name, h) => {
    assert.strictEqual(h, headers);
    seen.push(name);
  });
  assert.deepStrictEqual(seen, ['set-cookie', 'set-cookie', 'x-b']);
  assert.throws(() => headers.get('bad name'), TypeError);
  assert.throws(() => headers.get.call({}, 'a'), TypeError);
  assert.throws(() => headers.set('x', '\u0100'), TypeError);
  // A ServerHeaders object is a valid ServerHeadersInit.
  const copy = new ServerResponse(null, { headers });
  assert.deepStrictEqual(copy.headers.getSetCookie(), ['a=1', 'b=2']);
  const fromHeaders = new ServerResponse(null, { headers: new Headers({ q: 'r' }) });
  assert.strictEqual(fromHeaders.headers.get('q'), 'r');
});
