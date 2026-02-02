'use strict';

const common = require('../common');
const { Writable, Readable, Duplex } = require('stream');
const assert = require('assert');

{
  // Multiple callback.
  new Writable({
    construct: common.mustCall((callback) => {
      callback();
      callback();
    })
  }).on('error', common.expectsError({
    name: 'Error',
    code: 'ERR_MULTIPLE_CALLBACK'
  }));
}

{
  // Multiple callback.
  new Readable({
    construct: common.mustCall((callback) => {
      callback();
      callback();
    })
  }).on('error', common.expectsError({
    name: 'Error',
    code: 'ERR_MULTIPLE_CALLBACK'
  }));
}

{
  // Synchronous error.

  new Writable({
    construct: common.mustCall((callback) => {
      callback(new Error('test'));
    })
  }).on('error', common.expectsError({
    name: 'Error',
    message: 'test'
  }));
}

{
  // Synchronous error.

  new Readable({
    construct: common.mustCall((callback) => {
      callback(new Error('test'));
    })
  }).on('error', common.expectsError({
    name: 'Error',
    message: 'test'
  }));
}

{
  // Asynchronous error.

  new Writable({
    construct: common.mustCall((callback) => {
      process.nextTick(callback, new Error('test'));
    })
  }).on('error', common.expectsError({
    name: 'Error',
    message: 'test'
  }));
}

{
  // Asynchronous error.

  new Readable({
    construct: common.mustCall((callback) => {
      process.nextTick(callback, new Error('test'));
    })
  }).on('error', common.expectsError({
    name: 'Error',
    message: 'test'
  }));
}

function testDestroy(factory) {
  {
    let constructed = false;
    const s = factory({
      construct: common.mustCall((cb) => {
        constructed = true;
        process.nextTick(cb);
      })
    });
    s.on('close', common.mustCall(() => {
      assert.strictEqual(constructed, true);
    }));
    s.destroy();
  }

  {
    let constructed = false;
    const s = factory({
      construct: common.mustCall((cb) => {
        constructed = true;
        process.nextTick(cb);
      })
    });
    s.on('close', common.mustCall(() => {
      assert.strictEqual(constructed, true);
    }));
    s.destroy(null, () => {
      assert.strictEqual(constructed, true);
    });
  }

  {
    let constructed = false;
    const s = factory({
      construct: common.mustCall((cb) => {
        constructed = true;
        process.nextTick(cb);
      })
    });
    s.on('close', common.mustCall(() => {
      assert.strictEqual(constructed, true);
    }));
    s.destroy();
  }


  {
    let constructed = false;
    const s = factory({
      construct: common.mustCall((cb) => {
        constructed = true;
        process.nextTick(cb);
      })
    });
    s.on('close', common.mustCall(() => {
      assert.strictEqual(constructed, true);
    }));
    s.on('error', common.mustCall((err) => {
      assert.strictEqual(err.message, 'kaboom');
    }));
    s.destroy(new Error('kaboom'), (err) => {
      assert.strictEqual(err.message, 'kaboom');
      assert.strictEqual(constructed, true);
    });
  }

  {
    let constructed = false;
    const s = factory({
      construct: common.mustCall((cb) => {
        constructed = true;
        process.nextTick(cb);
      })
    });
    s.on('error', common.mustCall(() => {
      assert.strictEqual(constructed, true);
    }));
    s.on('close', common.mustCall(() => {
      assert.strictEqual(constructed, true);
    }));
    s.destroy(new Error());
  }
}
testDestroy((opts) => new Readable({
  read: common.mustNotCall(),
  ...opts
}));
testDestroy((opts) => new Writable({
  write: common.mustNotCall(),
  final: common.mustNotCall(),
  ...opts
}));

{
  let constructed = false;
  const r = new Readable({
    autoDestroy: true,
    construct: common.mustCall((cb) => {
      constructed = true;
      process.nextTick(cb);
    }),
    read: common.mustCall(() => {
      assert.strictEqual(constructed, true);
      r.push(null);
    })
  });
  r.on('close', common.mustCall(() => {
    assert.strictEqual(constructed, true);
  }));
  r.on('data', common.mustNotCall());
}

{
  let constructed = false;
  const w = new Writable({
    autoDestroy: true,
    construct: common.mustCall((cb) => {
      constructed = true;
      process.nextTick(cb);
    }),
    write: common.mustCall((chunk, encoding, cb) => {
      assert.strictEqual(constructed, true);
      process.nextTick(cb);
    }),
    final: common.mustCall((cb) => {
      assert.strictEqual(constructed, true);
      process.nextTick(cb);
    })
  });
  w.on('close', common.mustCall(() => {
    assert.strictEqual(constructed, true);
  }));
  w.end('data');
}

{
  let constructed = false;
  const w = new Writable({
    autoDestroy: true,
    construct: common.mustCall((cb) => {
      constructed = true;
      process.nextTick(cb);
    }),
    write: common.mustNotCall(),
    final: common.mustCall((cb) => {
      assert.strictEqual(constructed, true);
      process.nextTick(cb);
    })
  });
  w.on('close', common.mustCall(() => {
    assert.strictEqual(constructed, true);
  }));
  w.end();
}

{
  new Duplex({
    construct: common.mustCall()
  });
}

{
  // https://github.com/nodejs/node/issues/34448

  let constructed = false;
  const d = new Duplex({
    readable: false,
    construct: common.mustCall((callback) => {
      setImmediate(common.mustCall(() => {
        constructed = true;
        callback();
      }));
    }),
    write(chunk, encoding, callback) {
      callback();
    },
    read() {
      this.push(null);
    }
  });
  d.resume();
  d.end('foo');
  d.on('close', common.mustCall(() => {
    assert.strictEqual(constructed, true);
  }));
}

{
  // Construct should not cause stream to read.
  new Readable({
    construct: common.mustCall((callback) => {
      callback();
    }),
    read: common.mustNotCall()
  });
}
