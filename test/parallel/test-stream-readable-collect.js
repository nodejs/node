'use strict';

const common = require('../common');

const { Readable } = require('stream');
const assert = require('assert');

const hello = Buffer.from('hello, ');
const world = Buffer.from('world');

async function tests() {
  { // buffer, no limit
    const r = new Readable();

    r.push(hello);
    r.push(world);
    r.push(null);

    const result = await r.collect();

    assert.strictEqual(result.compare(Buffer.from('hello, world')), 0);
  }

  { // buffer, under limit
    const r = new Readable();

    r.push(hello);
    r.push(world);
    r.push(null);

    await assert.doesNotReject(r.collect(50));
  }

  { // buffer, over limit
    const r = new Readable();
    r.push(hello);
    r.push(world);
    r.push(null);

    await assert.rejects(r.collect(6), {
      code: 'ERR_STREAM_COLLECT_LIMIT_EXCEEDED',
    });
  }

  { // buffer, setEncoding, no limit
    const r = new Readable();

    r.setEncoding('utf8');

    r.push(hello);
    r.push(world);
    r.push(null);

    const result = await r.collect();

    assert.strictEqual(result, 'hello, world');
  }

  { // buffer, setEncoding, under limit
    const r = new Readable();

    r.setEncoding('utf8');

    r.push(hello);
    r.push(world);
    r.push(null);

    await assert.doesNotReject(r.collect(50));
  }

  { // buffer, setEncoding, over limit
    const r = new Readable();

    r.setEncoding('utf8');

    r.push(hello);
    r.push(world);
    r.push(null);

    await assert.rejects(r.collect(6), {
      code: 'ERR_STREAM_COLLECT_LIMIT_EXCEEDED',
    });
  }

  { // object, no limit
    const r = new Readable({ objectMode: true });

    const objs = [{ n: 0 }, { n: 1 }];

    r.push(objs[0]);
    r.push(objs[1]);
    r.push(null);

    const result = await r.collect();

    assert.deepStrictEqual(result, objs);
  }

  { // object, under limit
    const r = new Readable({ objectMode: true });

    const objs = [{ n: 0 }, { n: 1 }];

    r.push(objs[0]);
    r.push(objs[1]);
    r.push(null);

    assert.doesNotReject(r.collect(5));
  }

  { // object, over limit
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
