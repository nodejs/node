'use strict';

require('../common');
const { BroadcastChannel } = require('worker_threads');
const { inspect } = require('util');
const assert = require('assert');

// This test checks BroadcastChannel custom inspect outputs

{
  const bc = new BroadcastChannel('name');
  assert.throws(() => bc[inspect.custom].call(), {
    code: 'ERR_INVALID_THIS',
  });
  bc.close();
}

{
  const bc = new BroadcastChannel('name');
  assert.strictEqual(inspect(bc, { depth: -1 }), 'BroadcastChannel');
  bc.close();
}

{
  const bc = new BroadcastChannel('name');
  assert.strictEqual(
    inspect(bc),
    "BroadcastChannel { name: 'name', active: true }"
  );
  bc.close();
}

{
  const bc = new BroadcastChannel('name');
  assert.strictEqual(
    inspect(bc, { depth: null }),
    "BroadcastChannel { name: 'name', active: true }"
  );
  bc.close();
}
