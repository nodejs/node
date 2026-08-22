'use strict';

// Tests for the URLPattern constructor / test / exec C++ fast paths.

require('../common');
const assert = require('assert');
const { URLPattern } = require('url');

{
  const pattern = new URLPattern('https://example.com/foo');
  assert.strictEqual(pattern.protocol, 'https');
  assert.strictEqual(pattern.hostname, 'example.com');
  assert.strictEqual(pattern.pathname, '/foo');
  assert.strictEqual(pattern.test('https://example.com/foo'), true);
  assert.strictEqual(pattern.test('https://example.com/bar'), false);
  const result = pattern.exec('https://example.com/foo');
  assert.strictEqual(result.hostname.input, 'example.com');
  assert.strictEqual(result.pathname.input, '/foo');
  assert.strictEqual(result.protocol.input, 'https');
}

{
  const pattern = new URLPattern('/foo', 'https://example.com');
  assert.strictEqual(pattern.protocol, 'https');
  assert.strictEqual(pattern.hostname, 'example.com');
  assert.strictEqual(pattern.pathname, '/foo');
  assert.strictEqual(pattern.test('https://example.com/foo'), true);
  assert.strictEqual(pattern.test('/foo', 'https://example.com'), true);
  const result = pattern.exec('/foo', 'https://example.com');
  assert.strictEqual(result.hostname.input, 'example.com');
  assert.strictEqual(result.pathname.input, '/foo');
}

{
  const pattern = new URLPattern({
    pathname: '/foo',
    search: 'bar',
    hash: 'baz',
    baseURL: 'https://example.com:8080',
  });
  assert.strictEqual(pattern.protocol, 'https');
  assert.strictEqual(pattern.hostname, 'example.com');
  assert.strictEqual(pattern.port, '8080');
  assert.strictEqual(pattern.pathname, '/foo');
  assert.strictEqual(pattern.search, 'bar');
  assert.strictEqual(pattern.hash, 'baz');
  assert.strictEqual(
    pattern.test('https://example.com:8080/foo?bar#baz'),
    true,
  );
}

{
  // Non-string init members are ignored.
  const pattern = new URLPattern({ pathname: '/foo', port: 8080 });
  assert.strictEqual(pattern.pathname, '/foo');
  assert.strictEqual(pattern.port, '*');
}

{
  const pattern = new URLPattern({
    hostname: 'xn--caf-dma.com',
    pathname: '/café',
  });
  assert.strictEqual(pattern.hostname, 'xn--caf-dma.com');
  assert.strictEqual(pattern.pathname, '/caf%C3%A9');
  assert.strictEqual(
    pattern.test({ hostname: 'xn--caf-dma.com', pathname: '/café' }),
    true,
  );
  const result = pattern.exec({
    hostname: 'xn--caf-dma.com',
    pathname: '/café',
  });
  assert.strictEqual(result.hostname.input, 'xn--caf-dma.com');
}

{
  const pattern = new URLPattern({ pathname: '/:value' });
  const result = pattern.exec('https://example.com/test');
  assert.strictEqual(result.pathname.groups.value, 'test');
  assert.strictEqual(result.pathname.input, '/test');
}

{
  const pattern = new URLPattern({ pathname: '/([a-z]+)' });
  assert.strictEqual(pattern.hasRegExpGroups, true);
  assert.strictEqual(pattern.test({ pathname: '/abc' }), true);
  assert.strictEqual(pattern.test({ pathname: '/123' }), false);
  const result = pattern.exec({ pathname: '/abc' });
  assert.strictEqual(result.pathname.groups['0'], 'abc');
}

{
  const pattern = new URLPattern('https://*.example.com/foo');
  assert.strictEqual(pattern.test('https://sub.example.com/foo'), true);
  assert.strictEqual(pattern.test('https://example.com/foo'), false);
  const result = pattern.exec('https://sub.example.com/foo');
  assert.strictEqual(result.hostname.input, 'sub.example.com');
  assert.strictEqual(result.pathname.input, '/foo');
}

{
  // Constructor-string pattern used by the urlpattern-* benchmarks.
  const pattern = new URLPattern('https://(sub.)?example(.com/)foo');
  assert.strictEqual(pattern.hostname, '(sub.)?example(.com/)foo');
  assert.strictEqual(pattern.test('https://sub.example.com/foo'), false);
  assert.strictEqual(pattern.exec('https://sub.example.com/foo'), null);
}

{
  const pattern = new URLPattern({ pathname: '/FOO' }, { ignoreCase: true });
  assert.strictEqual(pattern.test({ pathname: '/foo' }), true);
  assert.strictEqual(pattern.test({ pathname: '/FOO' }), true);
}

{
  const pattern = new URLPattern();
  assert.strictEqual(pattern.protocol, '*');
  assert.strictEqual(pattern.test('https://example.com/'), true);
  assert.strictEqual(pattern.test(undefined), true);
  assert.notStrictEqual(pattern.exec('https://example.com/'), null);
}

{
  const pattern = new URLPattern({ protocol: 'https' });
  assert.strictEqual(pattern.test('https://example.com', undefined), true);
  assert.strictEqual(pattern.test('https://example.com', null), false);
  assert.strictEqual(pattern.exec('https://example.com', null), null);
}
