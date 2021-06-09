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

    await assert.rejects(r.collect(6), {
      code: 'ERR_STREAM_COLLECT_LIMIT_EXCEEDED',
    });
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
    await r.collect(50);
  }

  { // Buffer, setEncoding, over limit
    const r = new Readable();

    r.setEncoding('utf8');

    r.push(hello);
    r.push(world);
    r.push(null);

    await assert.rejects(r.collect(6), {
      code: 'ERR_STREAM_COLLECT_LIMIT_EXCEEDED',
    });
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
    await r.collect(5);
  }

  { // Object, over limit
    const r = new Readable({ objectMode: true });

    const objs = [{ n: 0 }, { n: 1 }, { n: 2 }];

    r.push(objs[0]);
    r.push(objs[1]);
    r.push(objs[2]);
    r.push(null);

    await assert.rejects(r.collect(2), {
      code: 'ERR_STREAM_COLLECT_LIMIT_EXCEEDED',
    });
  }
}

tests().then(common.mustCall());
