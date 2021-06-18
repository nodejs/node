// Flags: --expose-internals

'use strict';

const common = require('../common');
const {
  Readable,
  Transform,
  Writable,
  finished,
  PassThrough
} = require('stream');
const compose = require('internal/streams/compose');
const assert = require('assert');

{
  let res = '';
  compose(
    new Transform({
      transform: common.mustCall((chunk, encoding, callback) => {
        callback(null, chunk + chunk);
      })
    }),
    new Transform({
      transform: common.mustCall((chunk, encoding, callback) => {
        callback(null, chunk.toString().toUpperCase());
      })
    })
  )
  .end('asd')
  .on('data', common.mustCall((buf) => {
    res += buf;
  }))
  .on('end', common.mustCall(() => {
    assert.strictEqual(res, 'ASDASD');
  }));
}

{
  let res = '';
  compose(
    async function*(source) {
      for await (const chunk of source) {
        yield chunk + chunk;
      }
    },
    async function*(source) {
      for await (const chunk of source) {
        yield chunk.toString().toUpperCase();
      }
    }
  )
  .end('asd')
  .on('data', common.mustCall((buf) => {
    res += buf;
  }))
  .on('end', common.mustCall(() => {
    assert.strictEqual(res, 'ASDASD');
  }));
}

{
  let res = '';
  compose(
    async function*(source) {
      for await (const chunk of source) {
        yield chunk + chunk;
      }
    }
  )
  .end('asd')
  .on('data', common.mustCall((buf) => {
    res += buf;
  }))
  .on('end', common.mustCall(() => {
    assert.strictEqual(res, 'asdasd');
  }));
}

{
  let res = '';
  compose(
    Readable.from(['asd']),
    new Transform({
      transform: common.mustCall((chunk, encoding, callback) => {
        callback(null, chunk.toString().toUpperCase());
      })
    })
  )
  .on('data', common.mustCall((buf) => {
    res += buf;
  }))
  .on('end', common.mustCall(() => {
    assert.strictEqual(res, 'ASD');
  }));
}

{
  let res = '';
  compose(
    async function* () {
      yield 'asd';
    }(),
    new Transform({
      transform: common.mustCall((chunk, encoding, callback) => {
        callback(null, chunk.toString().toUpperCase());
      })
    })
  )
  .on('data', common.mustCall((buf) => {
    res += buf;
  }))
  .on('end', common.mustCall(() => {
    assert.strictEqual(res, 'ASD');
  }));
}

{
  let res = '';
  compose(
    new Transform({
      transform: common.mustCall((chunk, encoding, callback) => {
        callback(null, chunk.toString().toUpperCase());
      })
    }),
    async function*(source) {
      for await (const chunk of source) {
        yield chunk;
      }
    },
    new Writable({
      write: common.mustCall((chunk, encoding, callback) => {
        res += chunk;
        callback(null);
      })
    })
  )
  .end('asd')
  .on('finish', common.mustCall(() => {
    assert.strictEqual(res, 'ASD');
  }));
}

{
  let res = '';
  compose(
    new Transform({
      transform: common.mustCall((chunk, encoding, callback) => {
        callback(null, chunk.toString().toUpperCase());
      })
    }),
    async function*(source) {
      for await (const chunk of source) {
        yield chunk;
      }
    },
    async function(source) {
      for await (const chunk of source) {
        res += chunk;
      }
    }
  )
  .end('asd')
  .on('finish', common.mustCall(() => {
    assert.strictEqual(res, 'ASD');
  }));
}

{
  let res;
  compose(
    new Transform({
      objectMode: true,
      transform: common.mustCall((chunk, encoding, callback) => {
        callback(null, { chunk });
      })
    }),
    async function*(source) {
      for await (const chunk of source) {
        yield chunk;
      }
    },
    new Transform({
      objectMode: true,
      transform: common.mustCall((chunk, encoding, callback) => {
        callback(null, { chunk });
      })
    })
  )
  .end(true)
  .on('data', common.mustCall((buf) => {
    res = buf;
  }))
  .on('end', common.mustCall(() => {
    assert.strictEqual(res.chunk.chunk, true);
  }));
}

{
  const _err = new Error('asd');
  compose(
    new Transform({
      objectMode: true,
      transform: common.mustCall((chunk, encoding, callback) => {
        callback(_err);
      })
    }),
    async function*(source) {
      for await (const chunk of source) {
        yield chunk;
      }
    },
    new Transform({
      objectMode: true,
      transform: common.mustNotCall((chunk, encoding, callback) => {
        callback(null, { chunk });
      })
    })
  )
  .end(true)
  .on('data', common.mustNotCall())
  .on('end', common.mustNotCall())
  .on('error', (err) => {
    assert.strictEqual(err, _err);
  });
}

{
  const _err = new Error('asd');
  compose(
    new Transform({
      objectMode: true,
      transform: common.mustCall((chunk, encoding, callback) => {
        callback(null, chunk);
      })
    }),
    async function*(source) {
      let tmp = '';
      for await (const chunk of source) {
        tmp += chunk;
        throw _err;
      }
      return tmp;
    },
    new Transform({
      objectMode: true,
      transform: common.mustNotCall((chunk, encoding, callback) => {
        callback(null, { chunk });
      })
    })
  )
  .end(true)
  .on('data', common.mustNotCall())
  .on('end', common.mustNotCall())
  .on('error', (err) => {
    assert.strictEqual(err, _err);
  });
}

{
  let buf = '';

  // Convert into readable Duplex.
  const s1 = compose(async function* () {
    yield 'Hello';
    yield 'World';
  }(), async function* (source) {
    for await (const chunk of source) {
      yield String(chunk).toUpperCase();
    }
  }, async function(source) {
    for await (const chunk of source) {
      buf += chunk;
    }
  });

  assert.strictEqual(s1.writable, false);
  assert.strictEqual(s1.readable, false);

  finished(s1.resume(), common.mustCall((err) => {
    assert(!err);
    assert.strictEqual(buf, 'HELLOWORLD');
  }));
}

{
  let buf = '';
  // Convert into transform duplex.
  const s2 = compose(async function* (source) {
    for await (const chunk of source) {
      yield String(chunk).toUpperCase();
    }
  });
  s2.end('helloworld');
  s2.resume();
  s2.on('data', (chunk) => {
    buf += chunk;
  });

  finished(s2.resume(), common.mustCall((err) => {
    assert(!err);
    assert.strictEqual(buf, 'HELLOWORLD');
  }));
}

{
  let buf = '';

  // Convert into readable Duplex.
  const s1 = compose(async function* () {
    yield 'Hello';
    yield 'World';
  }());

  // Convert into transform duplex.
  const s2 = compose(async function* (source) {
    for await (const chunk of source) {
      yield String(chunk).toUpperCase();
    }
  });

  // Convert into writable duplex.
  const s3 = compose(async function(source) {
    for await (const chunk of source) {
      buf += chunk;
    }
  });

  const s4 = compose(s1, s2, s3);

  finished(s4, common.mustCall((err) => {
    assert(!err);
    assert.strictEqual(buf, 'HELLOWORLD');
  }));
}

{
  let buf = '';

  // Convert into readable Duplex.
  const s1 = compose(async function* () {
    yield 'Hello';
    yield 'World';
  }(), async function* (source) {
    for await (const chunk of source) {
      yield String(chunk).toUpperCase();
    }
  }, async function(source) {
    for await (const chunk of source) {
      buf += chunk;
    }
  });

  finished(s1, common.mustCall((err) => {
    assert(!err);
    assert.strictEqual(buf, 'HELLOWORLD');
  }));
}

{
  try {
    compose();
  } catch (err) {
    assert.strictEqual(err.code, 'ERR_MISSING_ARGS');
  }
}

{
  try {
    compose(new Writable(), new PassThrough());
  } catch (err) {
    assert.strictEqual(err.code, 'ERR_INVALID_ARG_VALUE');
  }
}

{
  try {
    compose(new PassThrough(), new Readable({ read() {} }), new PassThrough());
  } catch (err) {
    assert.strictEqual(err.code, 'ERR_INVALID_ARG_VALUE');
  }
}

{
  let buf = '';

  // Convert into readable Duplex.
  const s1 = compose(async function* () {
    yield 'Hello';
    yield 'World';
  }(), async function* (source) {
    for await (const chunk of source) {
      yield String(chunk).toUpperCase();
    }
  }, async function(source) {
    for await (const chunk of source) {
      buf += chunk;
    }
    return buf;
  });

  finished(s1, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_INVALID_RETURN_VALUE');
  }));
}

{
  let buf = '';

  // Convert into readable Duplex.
  const s1 = compose('HelloWorld', async function* (source) {
    for await (const chunk of source) {
      yield String(chunk).toUpperCase();
    }
  }, async function(source) {
    for await (const chunk of source) {
      buf += chunk;
    }
  });

  finished(s1, common.mustCall((err) => {
    assert(!err);
    assert.strictEqual(buf, 'HELLOWORLD');
  }));
}
