'use strict';

require('../common');
const { IncomingMessage } = require('http');
const assert = require('assert');

// Headers setter function set a header correctly
{
  const im = new IncomingMessage();
  im.headers = { key: 'value' };
  assert.deepStrictEqual(im.headers, { key: 'value' });
}

// Trailers setter function set a header correctly
{
  const im = new IncomingMessage();
  im.trailers = { key: 'value' };
  assert.deepStrictEqual(im.trailers, { key: 'value' });
}

// _addHeaderLines function set a header correctly
{
  const im = new IncomingMessage();
  im.headers = { key1: 'value1' };
  im._addHeaderLines(['key2', 'value2'], 2);
  assert.deepStrictEqual(im.headers, { key1: 'value1', key2: 'value2' });
}
