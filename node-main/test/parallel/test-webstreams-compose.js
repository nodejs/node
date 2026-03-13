'use strict';

const common = require('../common');
const assert = require('assert');

const {
  Transform,
  Readable,
  Writable,
  compose
} = require('stream');

const {
  TransformStream,
  ReadableStream,
  WritableStream,
} = require('stream/web');

{
  let res = '';

  const d = compose(
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk?.toString()?.replace(' ', '_'));
      })
    }),
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk?.toString()?.toUpperCase());
      })
    })
  );

  d.on('data', common.mustCall((chunk) => {
    res += chunk;
  }));

  d.on('end', common.mustCall(() => {
    assert.strictEqual(res, 'HELLO_WORLD');
  }));

  d.end('hello world');
}

{
  let res = '';

  compose(
    new Transform({
      transform: common.mustCall((chunk, encoding, callback) => {
        callback(null, chunk + chunk);
      })
    }),
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk.toString().toUpperCase());
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
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk.toString().toUpperCase());
      }),
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
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk.toString().toUpperCase());
      }),
    }),
    async function*(source) {
      for await (const chunk of source) {
        yield chunk + chunk;
      }
    },
    new Transform({
      transform: common.mustCall((chunk, enc, clb) => {
        clb(null, chunk?.toString()?.replaceAll('A', 'B'));
      })
    })
  )
  .end('asd')
  .on('data', common.mustCall((buf) => {
    res += buf;
  }))
  .on('end', common.mustCall(() => {
    assert.strictEqual(res, 'BSDBSD');
  }));
}

{
  let res = '';

  compose(
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk.toString().toUpperCase());
      }),
    }),
    async function*(source) {
      for await (const chunk of source) {
        yield chunk + chunk;
      }
    },
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk?.toString()?.replaceAll('A', 'B'));
      })
    })
  )
  .end('asd')
  .on('data', common.mustCall((buf) => {
    res += buf;
  }))
  .on('end', common.mustCall(() => {
    assert.strictEqual(res, 'BSDBSD');
  }));
}

{
  let res = '';
  compose(
    new ReadableStream({
      start(controller) {
        controller.enqueue('asd');
        controller.close();
      }
    }),
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk?.toString()?.toUpperCase());
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
    new ReadableStream({
      start(controller) {
        controller.enqueue('asd');
        controller.close();
      }
    }),
    new Transform({
      transform: common.mustCall((chunk, enc, clb) => {
        clb(null, chunk?.toString()?.toUpperCase());
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
    Readable.from(['asd']),
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk?.toString()?.toUpperCase());
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
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk.toString().toUpperCase());
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
    new WritableStream({
      write: common.mustCall((chunk) => {
        res += chunk;
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
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk.toString().toUpperCase());
      })
    }),
    async function*(source) {
      for await (const chunk of source) {
        yield chunk;
      }
    },
    new WritableStream({
      write: common.mustCall((chunk) => {
        res += chunk;
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
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk.toString().toUpperCase());
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

  compose(
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.error(new Error('asd'));
      })
    }),
    new TransformStream({
      transform: common.mustNotCall()
    })
  )
  .on('data', common.mustNotCall())
  .on('end', common.mustNotCall())
  .on('error', (err) => {
    assert.strictEqual(err?.message, 'asd');
  })
  .end('xyz');
}

{

  compose(
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk);
      })
    }),
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.error(new Error('asd'));
      })
    })
  )
  .on('data', common.mustNotCall())
  .on('end', common.mustNotCall())
  .on('error', (err) => {
    assert.strictEqual(err?.message, 'asd');
  })
  .end('xyz');
}

{

  compose(
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk);
      })
    }),
    async function*(source) { // eslint-disable-line require-yield
      let tmp = '';
      for await (const chunk of source) {
        tmp += chunk;
        throw new Error('asd');
      }
      return tmp;
    },
    new TransformStream({
      transform: common.mustNotCall()
    })
  )
  .on('data', common.mustNotCall())
  .on('end', common.mustNotCall())
  .on('error', (err) => {
    assert.strictEqual(err?.message, 'asd');
  })
  .end('xyz');
}

{

  compose(
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.error(new Error('asd'));
      })
    }),
    new Transform({
      transform: common.mustNotCall()
    })
  )
  .on('data', common.mustNotCall())
  .on('end', common.mustNotCall())
  .on('error', (err) => {
    assert.strictEqual(err?.message, 'asd');
  })
  .end('xyz');
}

{

  compose(
    new Transform({
      transform: common.mustCall((chunk, enc, clb) => {
        clb(new Error('asd'));
      })
    }),
    new TransformStream({
      transform: common.mustNotCall()
    })
  )
  .on('data', common.mustNotCall())
  .on('end', common.mustNotCall())
  .on('error', (err) => {
    assert.strictEqual(err?.message, 'asd');
  })
  .end('xyz');
}

{
  compose(
    new ReadableStream({
      start(controller) {
        controller.enqueue(new Error('asd'));
      }
    }),
    new TransformStream({
      transform: common.mustNotCall()
    })
  )
  .on('data', common.mustNotCall())
  .on('end', common.mustNotCall())
  .on('error', (err) => {
    assert.strictEqual(err?.message, 'asd');
  })
  .end('xyz');
}

{
  compose(
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk.toString().toUpperCase());
      })
    }),
    new WritableStream({
      write: common.mustCall((chunk, controller) => {
        controller.error(new Error('asd'));
      })
    })
  )
  .on('error', (err) => {
    assert.strictEqual(err?.message, 'asd');
  })
  .end('xyz');
}

{
  compose(
    new TransformStream({
      transform: common.mustCall((chunk, controller) => {
        controller.enqueue(chunk.toString().toUpperCase());
      })
    }),
    async function*(source) {
      for await (const chunk of source) {
        yield chunk;
      }
    },
    async function(source) {
      throw new Error('asd');
    }
  ).on('error', (err) => {
    assert.strictEqual(err?.message, 'asd');
  }).end('xyz');
}
