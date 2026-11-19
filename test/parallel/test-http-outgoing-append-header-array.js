'use strict';

require('../common');
const assert = require('assert');
const { ServerResponse } = require('http');

// setHeader() stores a copy of an array value, so appendHeader() never
// mutates an array the caller passed in. Frozen arrays must not make
// appendHeader() throw.

function createResponse() {
  return new ServerResponse({
    method: 'GET',
    httpVersionMajor: 1,
    httpVersionMinor: 1,
  });
}

// setHeader() with an array, then appendHeader().
{
  const res = createResponse();
  const cookies = ['a=1'];
  res.setHeader('Set-Cookie', cookies);
  res.appendHeader('set-cookie', 'b=2');
  res.appendHeader('set-cookie', ['c=3', 'd=4']);
  assert.deepStrictEqual(cookies, ['a=1']);
  assert.deepStrictEqual(res.getHeader('set-cookie'),
                         ['a=1', 'b=2', 'c=3', 'd=4']);
}

// The response owns the stored array: changing the caller's array later has
// no effect, and getHeader() returns the stored array itself.
{
  const res = createResponse();
  const cookies = ['a=1'];
  res.setHeader('Set-Cookie', cookies);
  cookies.push('b=2');
  assert.deepStrictEqual(res.getHeader('set-cookie'), ['a=1']);
  res.getHeader('set-cookie').push('c=3');
  assert.deepStrictEqual(res.getHeader('set-cookie'), ['a=1', 'c=3']);
}

// setHeader() with a frozen array, then appendHeader().
{
  const res = createResponse();
  const cookies = Object.freeze(['a=1']);
  res.setHeader('Set-Cookie', cookies);
  res.appendHeader('set-cookie', 'b=2');
  assert.deepStrictEqual(res.getHeader('set-cookie'), ['a=1', 'b=2']);
}

// appendHeader() with an array on an unset field, then appendHeader().
{
  const res = createResponse();
  const cookies = Object.freeze(['a=1']);
  res.appendHeader('Set-Cookie', cookies);
  res.appendHeader('set-cookie', 'b=2');
  assert.deepStrictEqual(res.getHeader('set-cookie'), ['a=1', 'b=2']);
}

// setHeaders() with an array value, then appendHeader().
{
  const res = createResponse();
  const values = Object.freeze(['a', 'b']);
  res.setHeaders(new Map([['x-foo', values]]));
  res.appendHeader('x-foo', 'c');
  assert.deepStrictEqual(values, ['a', 'b']);
  assert.deepStrictEqual(res.getHeader('x-foo'), ['a', 'b', 'c']);
}

// writeHead() with a raw header array after setHeader(): duplicate names
// go through appendHeader().
{
  const res = createResponse();
  res.setHeader('X-Other', '1');
  const cookies = Object.freeze(['a=1']);
  res.writeHead(200, ['Set-Cookie', cookies, 'Set-Cookie', 'b=2']);
  assert.deepStrictEqual(res.getHeader('set-cookie'), ['a=1', 'b=2']);
}
