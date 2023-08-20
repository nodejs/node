'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable, Writable, Transform, pipeline, PassThrough } = require('stream');
const { pipeline: pipelinePromise } = require('stream/promises');
const { ReadableStream, WritableStream, TransformStream } = require('stream/web');
const http = require('http');

{
  const values = [];
  let c;
  const rs = new ReadableStream({
    start(controller) {
      c = controller;
    }
  });
  const ws = new WritableStream({
    write(chunk) {
      values.push(chunk);
    }
  });

  pipeline(rs, ws, common.mustSucceed(() => {
    assert.deepStrictEqual(values, ['hello', 'world']);
  }));

  c.enqueue('hello');
  c.enqueue('world');
  c.close();
}

{
  let c;
  const rs = new ReadableStream({
    start(controller) {
      c = controller;
    }
  });

  const ws = new WritableStream({
    write() { }
  });

  pipeline(rs, ws, common.mustCall((err) => {
    assert.strictEqual(err?.message, 'kaboom');
  }));

  c.error(new Error('kaboom'));
}

{
  let c;
  const values = [];
  const rs = new ReadableStream({
    start(controller) {
      c = controller;
    }
  });

  const ts = new TransformStream({
    transform(chunk, controller) {
      controller.enqueue(chunk?.toString().toUpperCase());
    }
  });

  const ws = new WritableStream({
    write(chunk) {
      values.push(chunk?.toString());
    }
  });

  pipeline(rs, ts, ws, common.mustSucceed(() => {
    assert.deepStrictEqual(values, ['HELLO', 'WORLD']);
  }));

  c.enqueue('hello');
  c.enqueue('world');
  c.close();
}

{
  function makeTransformStream() {
    return new TransformStream({
      transform(chunk, controller) {
        controller.enqueue(chunk?.toString());
      }
    });
  }

  let c;
  const rs = new ReadableStream({
    start(controller) {
      c = controller;
    }
  });

  const ws = new WritableStream({
    write() { }
  });

  pipeline(rs,
           makeTransformStream(),
           makeTransformStream(),
           makeTransformStream(),
           makeTransformStream(),
           ws,
           common.mustCall((err) => {
             assert.strictEqual(err?.message, 'kaboom');
           }));

  c.error(new Error('kaboom'));
}

{
  const values = [];

  const r = new Readable({
    read() { }
  });

  const ws = new WritableStream({
    write(chunk) {
      values.push(chunk?.toString());
    }
  });

  pipeline(r, ws, common.mustSucceed(() => {
    assert.deepStrictEqual(values, ['helloworld']);
  }));

  r.push('hello');
  r.push('world');
  r.push(null);
}

{
  const values = [];
  let c;
  const rs = new ReadableStream({
    start(controller) {
      c = controller;
    }
  });

  const w = new Writable({
    write(chunk, encoding, callback) {
      values.push(chunk?.toString());
      callback();
    }
  });

  pipeline(rs, w, common.mustSucceed(() => {
    assert.deepStrictEqual(values, ['hello', 'world']);
  }));

  c.enqueue('hello');
  c.enqueue('world');
  c.close();
}

{
  const values = [];
  let c;
  const rs = new ReadableStream({
    start(controller) {
      c = controller;
    }
  });

  const ws = new WritableStream({
    write(chunk) {
      values.push(chunk?.toString());
    }
  });

  const t = new Transform({
    transform(chunk, encoding, callback) {
      callback(null, chunk?.toString().toUpperCase());
    }
  });

  pipeline(rs, t, ws, common.mustSucceed(() => {
    assert.deepStrictEqual(values, ['HELLOWORLD']);
  }));

  c.enqueue('hello');
  c.enqueue('world');
  c.close();
}

{
  const server = http.createServer((req, res) => {
    const rs = new ReadableStream({
      start(controller) {
        controller.enqueue('hello');
        controller.enqueue('world');
        controller.close();
      }
    });
    pipeline(rs, res, common.mustSucceed(() => {}));
  });

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port
    });
    req.end();
    const values = [];
    req.on('response', (res) => {
      res.on('data', (chunk) => {
        values.push(chunk?.toString());
      });
      res.on('end', common.mustCall(() => {
        assert.deepStrictEqual(values, ['hello', 'world']);
        server.close();
      }));
    });
  }));
}

{
  const values = [];
  const server = http.createServer((req, res) => {
    const ts = new TransformStream({
      transform(chunk, controller) {
        controller.enqueue(chunk?.toString().toUpperCase());
      }
    });
    pipeline(req, ts, res, common.mustSucceed());
  });

  server.listen(0, () => {
    const req = http.request({
      port: server.address().port,
      method: 'POST',
    });


    const rs = new ReadableStream({
      start(controller) {
        controller.enqueue('hello');
        controller.close();
      }
    });

    pipeline(rs, req, common.mustSucceed());

    req.on('response', (res) => {
      res.on('data', (chunk) => {
        values.push(chunk?.toString());
      }
      );
      res.on('end', common.mustCall(() => {
        assert.deepStrictEqual(values, ['HELLO']);
        server.close();
      }));
    });
  });
}

{
  const values = [];
  let c;
  const rs = new ReadableStream({
    start(controller) {
      c = controller;
    }
  });
  const ws = new WritableStream({
    write(chunk) {
      values.push(chunk?.toString());
    }
  });

  pipelinePromise(rs, ws).then(common.mustCall(() => {
    assert.deepStrictEqual(values, ['hello', 'world']);
  }));

  c.enqueue('hello');
  c.enqueue('world');
  c.close();
}

{
  let c;
  const rs = new ReadableStream({
    start(controller) {
      c = controller;
    }
  });

  const ws = new WritableStream({
    write() { }
  });

  pipelinePromise(rs, ws).then(common.mustNotCall()).catch(common.mustCall((err) => {
    assert.strictEqual(err?.message, 'kaboom');
  }));

  c.error(new Error('kaboom'));
}

{
  const values = [];
  let c;
  const rs = new ReadableStream({
    start(controller) {
      c = controller;
    }
  });

  pipeline(rs, async function(source) {
    for await (const chunk of source) {
      values.push(chunk?.toString());
    }
  }, common.mustSucceed(() => {
    assert.deepStrictEqual(values, ['hello', 'world']);
  }));

  c.enqueue('hello');
  c.enqueue('world');
  c.close();
}

{
  const rs = new ReadableStream({
    start() {}
  });

  pipeline(rs, async function(source) {
    throw new Error('kaboom');
  }, (err) => {
    assert.strictEqual(err?.message, 'kaboom');
  });
}

{
  const values = [];
  let c;
  const rs = new ReadableStream({
    start(controller) {
      c = controller;
    }
  });

  const ts = new TransformStream({
    transform(chunk, controller) {
      controller.enqueue(chunk?.toString().toUpperCase());
    }
  });

  pipeline(rs, ts, async function(source) {
    for await (const chunk of source) {
      values.push(chunk?.toString());
    }
  }, common.mustSucceed(() => {
    assert.deepStrictEqual(values, ['HELLO', 'WORLD']);
  }));

  c.enqueue('hello');
  c.enqueue('world');
  c.close();
}

{
  const values = [];
  let c;
  const rs = new ReadableStream({
    start(controller) {
      c = controller;
    }
  });

  const ws = new WritableStream({
    write(chunk) {
      values.push(chunk?.toString());
    }
  });

  pipeline(rs, async function* (source) {
    for await (const chunk of source) {
      yield chunk?.toString().toUpperCase();
    }
  }, ws, common.mustSucceed(() => {
    assert.deepStrictEqual(values, ['HELLO', 'WORLD']);
  }));

  c.enqueue('hello');
  c.enqueue('world');
  c.close();
}

{
  let c;
  const rs = new ReadableStream({
    start(controller) {
      c = controller;
    }
  });

  const ws = new WritableStream({
    write(chunk) { }
  }, { highWaterMark: 0 });

  pipeline(rs, ws, common.mustNotCall());

  for (let i = 0; i < 10; i++) {
    c.enqueue(`${i}`);
  }
  c.close();
}

{
  const rs = new ReadableStream({
    start(controller) {
      controller.close();
    }
  });

  pipeline(rs, new PassThrough(), common.mustSucceed());
}
