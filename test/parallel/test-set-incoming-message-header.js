'use strict';

require('../common');
const { IncomingMessage } = require('http');
const assert = require('assert');

// Headers setter should be a No-Op
{
  const im = new IncomingMessage();
  im.headers = { key: 'value' };
  assert.deepStrictEqual(im.headers, {});
}

// Trailers setter should be a No-Op
{
  const im = new IncomingMessage();
  im.trailers = { key: 'value' };
  assert.deepStrictEqual(im.trailers, {});
}

// _addHeaderLines function set a header correctly
{
  const im = new IncomingMessage();
  im._addHeaderLines(['key2', 'value2'], 2);
  assert.deepStrictEqual(im.headers, { key2: 'value2' });
}
