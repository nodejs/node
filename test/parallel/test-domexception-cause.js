'use strict';

require('../common');
const { strictEqual, deepStrictEqual } = require('assert');

{
  const domException = new DOMException('no cause', 'abc');
  strictEqual(domException.name, 'abc');
  strictEqual('cause' in domException, false);
  strictEqual(domException.cause, undefined);
}

{
  const domException = new DOMException('with undefined cause', { name: 'abc', cause: undefined });
  strictEqual(domException.name, 'abc');
  strictEqual('cause' in domException, true);
  strictEqual(domException.cause, undefined);
}

{
  const domException = new DOMException('with string cause', { name: 'abc', cause: 'foo' });
  strictEqual(domException.name, 'abc');
  strictEqual('cause' in domException, true);
  strictEqual(domException.cause, 'foo');
}

{
  const object = { reason: 'foo' };
  const domException = new DOMException('with object cause', { name: 'abc', cause: object });
  strictEqual(domException.name, 'abc');
  strictEqual('cause' in domException, true);
  deepStrictEqual(domException.cause, object);
}
