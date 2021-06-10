'use strict';

const common = require('../common');

const { Readable } = require('stream');
const assert = require('assert');

const hello = Buffer.from('hello, ');
const world = Buffer.from('world');

async function tests() {
  { // Buffer, no limit
    const r = new Readable();

    r.push(hello);
    r.push(world);
    r.push(null);

    const result = await r.collect();

    assert.strictEqual(result.compare(Buffer.from('hello, world')), 0);
  }

  { // Buffer, under limit
    const r = new Readable();

    r.push(hello);
    r.push(world);
    r.push(null);

    // Does not reject
    await r.collect(50);
  }

  { // Buffer, over limit
    const r = new Readable();
    r.push(hello);
    r.push(world);
    r.push(null);

    await assert.rejects(r.collect({ limit: 10 }), {
      code: 'ERR_STREAM_COLLECT_LIMIT_EXCEEDED',
    });
    assert.ok(r.destroyed);
  }

  { // Buffer, over limit, rejectAtLimit: false
    const streamContents = [hello, world];
    const r = new Readable({
      read() {
        this.push(streamContents.shift() || null);
      }
    });

    assert.deepStrictEqual(
      await r.collect({ limit: 10, rejectAtLimit: false }),
      hello
    );
    assert.deepStrictEqual(await r.collect(), world);
  }

  { // Buffer, setEncoding, no limit
    const r = new Readable();

    r.setEncoding('utf8');

    r.push(hello);
    r.push(world);
    r.push(null);

    const result = await r.collect();

    assert.strictEqual(result, 'hello, world');
  }

  { // Buffer, setEncoding, under limit
    const r = new Readable();

    r.setEncoding('utf8');

    r.push(hello);
    r.push(world);
    r.push(null);

    // Does not reject
    await r.collect({ limit: 50 });
  }

  { // Buffer, setEncoding, over limit
    const r = new Readable();

    r.setEncoding('utf8');

    r.push(hello);
    r.push(world);
    r.push(null);

    await assert.rejects(r.collect({ limit: 10 }), {
      code: 'ERR_STREAM_COLLECT_LIMIT_EXCEEDED'
    });
    assert.ok(r.destroyed);
  }

  { // Buffer, setEncoding, over limit, rejectAtLimit: false
    const streamContents = [hello, world];
    const r = new Readable({
      read() {
        this.push(streamContents.shift() || null);
      }
    });
    r.setEncoding('utf8');

    assert.deepStrictEqual(
      await r.collect({ limit: 10, rejectAtLimit: false }),
      'hello, '
    );
    assert.deepStrictEqual(await r.collect(), 'world');
  }

  { // Object, no limit
    const r = new Readable({ objectMode: true });

    const objs = [{ n: 0 }, { n: 1 }];

    r.push(objs[0]);
    r.push(objs[1]);
    r.push(null);

    const result = await r.collect();

    assert.deepStrictEqual(result, objs);
  }

  { // Object, under limit
    const r = new Readable({ objectMode: true });

    const objs = [{ n: 0 }, { n: 1 }];

    r.push(objs[0]);
    r.push(objs[1]);
    r.push(null);

    // Does not reject
    await r.collect({ limit: 5 });
  }

  { // Object, over limit
    const r = new Readable({ objectMode: true });

    const objs = [{ n: 0 }, { n: 1 }, { n: 2 }];

    r.push(objs[0]);
    r.push(objs[1]);
    r.push(objs[2]);
    r.push(null);

    await assert.rejects(r.collect({ limit: 2 }), {
      code: 'ERR_STREAM_COLLECT_LIMIT_EXCEEDED'
    });
    assert.ok(r.destroyed);
  }

  { // Object, over limit, rejectAtLimit: false
    const r = new Readable({ objectMode: true });

    const objs = [{ n: 0 }, { n: 1 }, { n: 2 }];

    r.push(objs[0]);
    r.push(objs[1]);
    r.push(objs[2]);
    r.push(null);

    assert.deepStrictEqual(
      await r.collect({ limit: 2, rejectAtLimit: false }),
      [{ n: 0 }, { n: 1 }]
    );
    assert.deepStrictEqual(await r.collect(), [{ n: 2 }]);
  }

  { // Stream emits an error
    const error = new Error();
    const r = new Readable({
      read() {
        this.emit('error', error);
      }
    });

    const resultPromise = r.collect();

    r.emit('error', error);

    await assert.rejects(resultPromise, error);
    assert.ok(r.destroyed);
  }
}

tests().then(common.mustCall());
