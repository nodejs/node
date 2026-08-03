'use strict';

require('../common');
const assert = require('assert');

{
  const domException = new DOMException('no cause', 'abc');
  assert.strictEqual(domException.name, 'abc');
  assert.strictEqual('cause' in domException, false);
  assert.strictEqual(domException.cause, undefined);
}

{
  const domException = new DOMException('with undefined cause', { name: 'abc', cause: undefined });
  assert.strictEqual(domException.name, 'abc');
  assert.strictEqual('cause' in domException, true);
  assert.strictEqual(domException.cause, undefined);
}

{
  const domException = new DOMException('with string cause', { name: 'abc', cause: 'foo' });
  assert.strictEqual(domException.name, 'abc');
  assert.strictEqual('cause' in domException, true);
  assert.strictEqual(domException.cause, 'foo');
}

{
  const object = { reason: 'foo' };
  const domException = new DOMException('with object cause', { name: 'abc', cause: object });
  assert.strictEqual(domException.name, 'abc');
  assert.strictEqual('cause' in domException, true);
  assert.deepStrictEqual(domException.cause, object);
}
