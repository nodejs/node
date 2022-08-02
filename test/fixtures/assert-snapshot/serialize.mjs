import assert from 'node:assert';

function fn() {
  this.should.be.a.fn();
  return false;
}

await assert.snapshot(fn);
await assert.snapshot({ foo: "bar" });
