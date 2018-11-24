'use strict';

const common = require('../common');
const { Stream, Writable, Readable, Transform, pipeline } = require('stream');
const assert = require('assert');
const http = require('http');
const { promisify } = require('util');

{
  let finished = false;
  const processed = [];
  const expected = [
    Buffer.from('a'),
    Buffer.from('b'),
    Buffer.from('c')
  ];

  const read = new Readable({
    read() {}
  });

  const write = new Writable({
    write(data, enc, cb) {
      processed.push(data);
      cb();
    }
  });

  write.on('finish', () => {
    finished = true;
  });

  for (let i = 0; i < expected.length; i++) {
    read.push(expected[i]);
  }
  read.push(null);

  pipeline(read, write, common.mustCall((err) => {
    assert.ok(!err, 'no error');
    assert.ok(finished);
    assert.deepStrictEqual(processed, expected);
  }));
}

{
  const read = new Readable({
    read() {}
  });

  assert.throws(() => {
    pipeline(read, () => {});
  }, /ERR_MISSING_ARGS/);
  assert.throws(() => {
    pipeline(() => {});
  }, /ERR_MISSING_ARGS/);
  assert.throws(() => {
    pipeline();
  }, /ERR_INVALID_CALLBACK/);
}

{
  const read = new Readable({
    read() {}
  });

  const write = new Writable({
    write(data, enc, cb) {
      cb();
    }
  });

  read.push('data');
  setImmediate(() => read.destroy());

  pipeline(read, write, common.mustCall((err) => {
    assert.ok(err, 'should have an error');
  }));
}

{
  const read = new Readable({
    read() {}
  });

  const write = new Writable({
    write(data, enc, cb) {
      cb();
    }
  });

  read.push('data');
  setImmediate(() => read.destroy(new Error('kaboom')));

  const dst = pipeline(read, write, common.mustCall((err) => {
    assert.deepStrictEqual(err, new Error('kaboom'));
  }));

  assert.strictEqual(dst, write);
}

{
  const read = new Readable({
    read() {}
  });

  const transform = new Transform({
    transform(data, enc, cb) {
      cb(new Error('kaboom'));
    }
  });

  const write = new Writable({
    write(data, enc, cb) {
      cb();
    }
  });

  read.on('close', common.mustCall());
  transform.on('close', common.mustCall());
  write.on('close', common.mustCall());

  const dst = pipeline(read, transform, write, common.mustCall((err) => {
    assert.deepStrictEqual(err, new Error('kaboom'));
  }));

  assert.strictEqual(dst, write);

  read.push('hello');
}

{
  const server = http.createServer((req, res) => {
    const rs = new Readable({
      read() {
        rs.push('hello');
        rs.push(null);
      }
    });

    pipeline(rs, res, () => {});
  });

  server.listen(0, () => {
    const req = http.request({
      port: server.address().port
    });

    req.end();
    req.on('response', (res) => {
      const buf = [];
      res.on('data', (data) => buf.push(data));
      res.on('end', common.mustCall(() => {
        assert.deepStrictEqual(
          Buffer.concat(buf),
          Buffer.from('hello')
        );
        server.close();
      }));
    });
  });
}

{
  const server = http.createServer((req, res) => {
    let sent = false;
    const rs = new Readable({
      read() {
        if (sent) {
          return;
        }
        sent = true;
        rs.push('hello');
      },
      destroy: common.mustCall((err, cb) => {
        // prevents fd leaks by destroying http pipelines
        cb();
      })
    });

    pipeline(rs, res, () => {});
  });

  server.listen(0, () => {
    const req = http.request({
      port: server.address().port
    });

    req.end();
    req.on('response', (res) => {
      setImmediate(() => {
        res.destroy();
        server.close();
      });
    });
  });
}

{
  const server = http.createServer((req, res) => {
    let sent = 0;
    const rs = new Readable({
      read() {
        if (sent++ > 10) {
          return;
        }
        rs.push('hello');
      },
      destroy: common.mustCall((err, cb) => {
        cb();
      })
    });

    pipeline(rs, res, () => {});
  });

  let cnt = 10;

  const badSink = new Writable({
    write(data, enc, cb) {
      cnt--;
      if (cnt === 0) cb(new Error('kaboom'));
      else cb();
    }
  });

  server.listen(0, () => {
    const req = http.request({
      port: server.address().port
    });

    req.end();
    req.on('response', (res) => {
      pipeline(res, badSink, common.mustCall((err) => {
        assert.deepStrictEqual(err, new Error('kaboom'));
        server.close();
      }));
    });
  });
}

{
  const server = http.createServer((req, res) => {
    pipeline(req, res, common.mustCall());
  });

  server.listen(0, () => {
    const req = http.request({
      port: server.address().port
    });

    let sent = 0;
    const rs = new Readable({
      read() {
        if (sent++ > 10) {
          return;
        }
        rs.push('hello');
      }
    });

    pipeline(rs, req, common.mustCall(() => {
      server.close();
    }));

    req.on('response', (res) => {
      let cnt = 10;
      res.on('data', () => {
        cnt--;
        if (cnt === 0) rs.destroy();
      });
    });
  });
}

{
  const makeTransform = () => {
    const tr = new Transform({
      transform(data, enc, cb) {
        cb(null, data);
      }
    });

    tr.on('close', common.mustCall());
    return tr;
  };

  const rs = new Readable({
    read() {
      rs.push('hello');
    }
  });

  let cnt = 10;

  const ws = new Writable({
    write(data, enc, cb) {
      cnt--;
      if (cnt === 0) return cb(new Error('kaboom'));
      cb();
    }
  });

  rs.on('close', common.mustCall());
  ws.on('close', common.mustCall());

  pipeline(
    rs,
    makeTransform(),
    makeTransform(),
    makeTransform(),
    makeTransform(),
    makeTransform(),
    makeTransform(),
    ws,
    common.mustCall((err) => {
      assert.deepStrictEqual(err, new Error('kaboom'));
    })
  );
}

{
  const oldStream = new Stream();

  oldStream.pause = oldStream.resume = () => {};
  oldStream.write = (data) => {
    oldStream.emit('data', data);
    return true;
  };
  oldStream.end = () => {
    oldStream.emit('end');
  };

  const expected = [
    Buffer.from('hello'),
    Buffer.from('world')
  ];

  const rs = new Readable({
    read() {
      for (let i = 0; i < expected.length; i++) {
        rs.push(expected[i]);
      }
      rs.push(null);
    }
  });

  const ws = new Writable({
    write(data, enc, cb) {
      assert.deepStrictEqual(data, expected.shift());
      cb();
    }
  });

  let finished = false;

  ws.on('finish', () => {
    finished = true;
  });

  pipeline(
    rs,
    oldStream,
    ws,
    common.mustCall((err) => {
      assert(!err, 'no error');
      assert(finished, 'last stream finished');
    })
  );
}

{
  const oldStream = new Stream();

  oldStream.pause = oldStream.resume = () => {};
  oldStream.write = (data) => {
    oldStream.emit('data', data);
    return true;
  };
  oldStream.end = () => {
    oldStream.emit('end');
  };

  const destroyableOldStream = new Stream();

  destroyableOldStream.pause = destroyableOldStream.resume = () => {};
  destroyableOldStream.destroy = common.mustCall(() => {
    destroyableOldStream.emit('close');
  });
  destroyableOldStream.write = (data) => {
    destroyableOldStream.emit('data', data);
    return true;
  };
  destroyableOldStream.end = () => {
    destroyableOldStream.emit('end');
  };

  const rs = new Readable({
    read() {
      rs.destroy(new Error('stop'));
    }
  });

  const ws = new Writable({
    write(data, enc, cb) {
      cb();
    }
  });

  let finished = false;

  ws.on('finish', () => {
    finished = true;
  });

  pipeline(
    rs,
    oldStream,
    destroyableOldStream,
    ws,
    common.mustCall((err) => {
      assert.deepStrictEqual(err, new Error('stop'));
      assert(!finished, 'should not finish');
    })
  );
}

{
  const pipelinePromise = promisify(pipeline);

  async function run() {
    const read = new Readable({
      read() {}
    });

    const write = new Writable({
      write(data, enc, cb) {
        cb();
      }
    });

    read.push('data');
    read.push(null);

    let finished = false;

    write.on('finish', () => {
      finished = true;
    });

    await pipelinePromise(read, write);

    assert(finished);
  }

  run();
}

{
  const read = new Readable({
    read() {}
  });

  const transform = new Transform({
    transform(data, enc, cb) {
      cb(new Error('kaboom'));
    }
  });

  const write = new Writable({
    write(data, enc, cb) {
      cb();
    }
  });

  assert.throws(
    () => pipeline(read, transform, write),
    { code: 'ERR_INVALID_CALLBACK' }
  );
}
