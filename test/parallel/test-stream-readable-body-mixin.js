'use strict';
const common = require('../common');
const assert = require('assert');
const Readable = require('stream').Readable;

{
  const r = new Readable({
    read() {
      this.push('asd');
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.on('end', common.mustCall());
  r.on('close', common.mustCall());
  r.text().then(common.mustCall((val) => {
    assert.strictEqual(r.bodyUsed, true);
    assert.strictEqual(val, 'asd');
  }));
  process.nextTick(() => {
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const n = ['a', 's', 'd', null];
  const r = new Readable({
    read() {
      this.push(n.shift());
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.on('end', common.mustCall());
  r.on('close', common.mustCall());
  r.text().then(common.mustCall((val) => {
    assert.strictEqual(r.bodyUsed, true);
    assert.strictEqual(val, 'asd');
  }));
  process.nextTick(() => {
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const r = new Readable({
    read() {
      this.push('asd');
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.on('error', common.mustCall(() => {
    assert.strictEqual(r.bodyUsed, true);
  }));
  r.on('close', common.mustCall());
  const _err = new Error();
  r.text().catch(common.mustCall((err) => assert.strictEqual(err, _err)));
  r.destroy(_err);
  process.nextTick(() => {
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const obj = { asd: '123' };
  const r = new Readable({
    read() {
      this.push(JSON.stringify(obj));
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.on('end', common.mustCall());
  r.on('close', common.mustCall());
  r.json().then(common.mustCall((val) => {
    assert.strictEqual(r.bodyUsed, true);
    assert.deepStrictEqual(val, obj);
  }));
  process.nextTick(() => {
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const r = new Readable({
    read() {
      this.push('asd');
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.on('error', common.mustCall(() => {
    assert.strictEqual(r.bodyUsed, true);
  }));
  r.on('close', common.mustCall());
  r.json()
    .catch(common.mustCall((err) => {
      assert.strictEqual(err.message,
                         'Unexpected token a in JSON at position 0');
      assert.strictEqual(r.bodyUsed, true);
    }));
  process.nextTick(() => {
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const r = new Readable({
    read() {
      this.push('asd');
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.on('error', common.mustCall(() => {
    assert.strictEqual(r.bodyUsed, true);
  }));
  r.on('close', common.mustCall());
  r.json().catch(common.mustCall((err) => {
    assert.strictEqual(r.bodyUsed, true);
    assert(err);
  }));
  process.nextTick(() => {
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const buf = Uint8Array.from('asd');
  const r = new Readable({
    read() {
      this.push(buf);
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.on('end', common.mustCall());
  r.on('close', common.mustCall());
  r.arrayBuffer()
    .then(common.mustCall((val) => assert.deepStrictEqual(val, buf)));
  process.nextTick(() => {
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const r = new Readable({
    read() {
      this.push('asd');
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.pause();
  assert.strictEqual(r.bodyUsed, false);
  r.on('data', common.mustCall(() => {
    assert.strictEqual(r.bodyUsed, true);
  }));
  r.on('end', common.mustCall());
  r.on('close', common.mustCall());
  r.text().then(common.mustCall((val) => assert.strictEqual(val, 'asd')));
  process.nextTick(() => {
    assert.strictEqual(r.bodyUsed, true);
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const n = ['a', 's', 'd', null];
  const r = new Readable({
    read() {
      this.push(n.shift());
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.pause();
  assert.strictEqual(r.bodyUsed, false);
  r.on('data', common.mustCallAtLeast(3));
  r.on('end', common.mustCall());
  r.on('close', common.mustCall());
  r.text().then(common.mustCall((val) => assert.strictEqual(val, 'asd')));
  process.nextTick(() => {
    assert.strictEqual(r.bodyUsed, true);
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const r = new Readable({
    read() {
      this.push('asd');
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.pause();
  assert.strictEqual(r.bodyUsed, false);
  r.on('data', common.mustNotCall());
  r.on('error', common.mustCall());
  r.on('close', common.mustCall());
  const _err = new Error();
  r.text().catch(common.mustCall((err) => assert.strictEqual(err, _err)));
  r.destroy(_err);
  process.nextTick(() => {
    assert.strictEqual(r.bodyUsed, true);
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const obj = { asd: '123' };
  const r = new Readable({
    read() {
      this.push(JSON.stringify(obj));
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.pause();
  assert.strictEqual(r.bodyUsed, false);
  r.on('data', common.mustCall(() => {
    assert.strictEqual(r.bodyUsed, true);
  }));
  r.on('end', common.mustCall());
  r.on('close', common.mustCall());
  r.json().then(common.mustCall((val) => assert.deepStrictEqual(val, obj)));
  process.nextTick(() => {
    assert.strictEqual(r.bodyUsed, true);
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const r = new Readable({
    read() {
      this.push('asd');
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.pause();
  assert.strictEqual(r.bodyUsed, false);
  r.on('data', common.mustCall(() => {
    assert.strictEqual(r.bodyUsed, true);
  }));
  r.on('error', common.mustCall(() => {
    assert.strictEqual(r.bodyUsed, true);
  }));
  r.on('close', common.mustCall());
  r.json()
    .catch(common.mustCall((err) => assert.strictEqual(
      err.message, 'Unexpected token a in JSON at position 0')));
  process.nextTick(() => {
    assert.strictEqual(r.bodyUsed, true);
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const buf = Uint8Array.from('asd');
  const r = new Readable({
    read() {
      this.push(buf);
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  r.pause();
  assert.strictEqual(r.bodyUsed, false);
  r.on('data', common.mustCall());
  r.on('end', common.mustCall());
  r.on('close', common.mustCall());
  r.arrayBuffer()
    .then(common.mustCall((val) => assert.deepStrictEqual(val, buf)));
  process.nextTick(() => {
    assert.strictEqual(r.bodyUsed, true);
    assert.strictEqual(r.isPaused(), false);
  });
}

{
  const buf = Uint8Array.from('asd');
  const r = new Readable({
    read() {
      this.push(buf);
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  assert.strictEqual(r.bodyUsed, false);
  r.on('data', common.mustCall(() => {
    assert.strictEqual(r.bodyUsed, true);
    r.json()
      .catch(common.mustCall((err) => assert(
        err instanceof TypeError)));
    r.arrayBuffer()
      .catch(common.mustCall((err) => assert(
        err instanceof TypeError)));
    r.blob()
      .catch(common.mustCall((err) => assert(
        err instanceof TypeError)));
    r.text()
      .catch(common.mustCall((err) => assert(
        err instanceof TypeError)));
  }));
  r.on('error', common.mustNotCall());
  r.on('end', common.mustCall());
  r.on('close', common.mustCall());
}

{
  const r = new Readable({
    read() {
      this.push(null);
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  assert.strictEqual(r.bodyUsed, false);
  r.on('end', common.mustCall(() => {
    assert.strictEqual(r.bodyUsed, true);
    r.json()
      .catch(common.mustCall((err) => assert(
        err instanceof TypeError)));
    r.arrayBuffer()
      .catch(common.mustCall((err) => assert(
        err instanceof TypeError)));
    r.blob()
      .catch(common.mustCall((err) => assert(
        err instanceof TypeError)));
    r.text()
      .catch(common.mustCall((err) => assert(
        err instanceof TypeError)));
  }));
  r.on('error', common.mustNotCall());
  r.on('data', common.mustNotCall());
  r.on('close', common.mustCall());
}

for (const key of ['text', 'json', 'arrayBuffer', 'blob']) {
  const r = new Readable({
    read() {
    }
  });
  assert.strictEqual(r.bodyUsed, false);
  assert.strictEqual(r.bodyUsed, false);
  r[key]()
    .catch(common.mustCall((err) => assert.strictEqual(
      err.name, 'AbortError')));
  r.destroy();
  r.on('error', common.mustNotCall());
  r.on('end', common.mustNotCall());
  r.on('data', common.mustNotCall());
  r.on('close', common.mustCall());

}

{
  const r = new Readable({
    read() {
      this.push(null);
    }
  });
  r.body = 'asd';
  assert.strictEqual(r.body, 'asd');
}
