'use strict';

require('../common');
const assert = require('assert');
const Client = require('_debugger').Client;

{
  const client = new Client();
  assert.deepStrictEqual(client.handles, {});
}

{
  const client = new Client();
  client._addHandle(null);
  assert.deepStrictEqual(client.handles, {});
}

{
  const client = new Client();
  client._addHandle('not an object');
  assert.deepStrictEqual(client.handles, {});
}

{
  const client = new Client();
  client._addHandle({ handle: 'not a number' });
  assert.deepStrictEqual(client.handles, {});
}

{
  const client = new Client();
  const validNoScript = { handle: 6, id: 'foo', type: 'not a script' };
  client._addHandle(validNoScript);
  assert.deepStrictEqual(client.handles, { 6: validNoScript });
  assert.deepStrictEqual(client.scripts, {});
}

{
  const client = new Client();
  const validWithScript = { handle: 5, id: 'bar', type: 'script' };
  client._addHandle(validWithScript);
  assert.deepStrictEqual(client.handles, { 5: validWithScript });
  assert.deepStrictEqual(client.scripts, { bar: validWithScript });
}
