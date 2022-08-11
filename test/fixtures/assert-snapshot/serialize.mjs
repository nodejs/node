import assert from 'node:assert';

function fn() {
  this.should.be.a.fn();
  return false;
}

await assert.snapshot(fn, 'function');
await assert.snapshot({ foo: "bar" }, 'object');
await assert.snapshot(new Set([1, 2, 3]), 'set');
await assert.snapshot(new Map([['one', 1], ['two', 2]]), 'map');
