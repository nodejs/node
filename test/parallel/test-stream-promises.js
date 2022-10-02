'use strict';

const common = require('../common');
const stream = require('stream');
const {
  Readable,
  Writable,
  promises,
} = stream;
const {
  finished,
  pipeline,
} = require('stream/promises');
const fs = require('fs');
const assert = require('assert');
const { promisify } = require('util');

assert.strictEqual(promises.pipeline, pipeline);
assert.strictEqual(promises.finished, finished);
assert.strictEqual(pipeline, promisify(stream.pipeline));
assert.strictEqual(finished, promisify(stream.finished));

// pipeline success
{
  let finished = false;
  const processed = [];
  const expected = [
    Buffer.from('a'),
    Buffer.from('b'),
    Buffer.from('c'),
  ];

  const read = new Readable({
    read() { }
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

  pipeline(read, write).then(common.mustCall((value) => {
    assert.ok(finished);
    assert.deepStrictEqual(processed, expected);
  }));
}

// pipeline error
{
  const read = new Readable({
    read() { }
  });

  const write = new Writable({
    write(data, enc, cb) {
      cb();
    }
  });

  read.push('data');
  setImmediate(() => read.destroy());

  pipeline(read, write).catch(common.mustCall((err) => {
    assert.ok(err, 'should have an error');
  }));
}

// finished success
{
  async function run() {
    const rs = fs.createReadStream(__filename);

    let ended = false;
    rs.resume();
    rs.on('end', () => {
      ended = true;
    });
    await finished(rs);
    assert(ended);
  }

  run().then(common.mustCall());
}

// finished error
{
  const rs = fs.createReadStream('file-does-not-exist');

  assert.rejects(finished(rs), {
    code: 'ENOENT'
  }).then(common.mustCall());
}

{
  const streamObj = new Readable();
  assert.throws(
    () => {
      // Passing autoCleanup option not as boolean
      // should throw error
      finished(streamObj, {autoCleanup: 2});
    },
    {code: 'ERR_INVALID_ARG_TYPE'}
  );
}

// Below code should not throw any errors as the
// streamObj is `Stream` and autoCleanup is boolean
{
  const streamObj = new Readable();
  finished(streamObj, {autoCleanup: true})
}


// cleanup function should not be called when autoCleanup is set to false
// listenerCount should be 1 after calling finish
{
  const streamObj = new Writable();
  assert(streamObj.listenerCount('end') === 0);
  finished(streamObj, {autoCleanup: false}).then(() => {
    assert(streamObj.listenerCount('end') === 1);
  })
}

// cleanup function should  be called when autoCleanup is set to true
// listenerCount should be 0 after calling finish
{
  const streamObj = new Writable();
  assert(streamObj.listenerCount('end') === 0);
  finished(streamObj, {autoCleanup: true}).then(() => {
    assert(streamObj.listenerCount('end') === 0);
  })
}

// cleanup function should not be called when autoCleanup has not been set
// listenerCount should be 1 after calling finish
{
  const streamObj = new Writable();
  assert(streamObj.listenerCount('end') === 0);
  finished(streamObj).then(() => {
    assert(streamObj.listenerCount('end') === 1);
  })
}
