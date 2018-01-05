'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');

const MAX = 42;
const BATCH = 10;

{
  const readable = new Readable({
    objectMode: true,
    read: common.mustCall(function() {
      console.log('>> READ');
      fetchData((err, data) => {
        if (err) {
          this.destroy(err);
          return;
        }

        if (data.length === 0) {
          console.log('pushing null');
          this.push(null);
          return;
        }

        console.log('pushing');
        data.forEach((d) => this.push(d));
      });
    }, Math.floor(MAX / BATCH) + 2)
  });

  let i = 0;
  function fetchData(cb) {
    if (i > MAX) {
      setTimeout(cb, 10, null, []);
    } else {
      const array = [];
      const max = i + BATCH;
      for (; i < max; i++) {
        array.push(i);
      }
      setTimeout(cb, 10, null, array);
    }
  }

  readable.on('readable', () => {
    let data;
    console.log('readable emitted');
    while (data = readable.read()) {
      console.log(data);
    }
  });

  readable.on('end', common.mustCall(() => {
    assert.strictEqual(i, (Math.floor(MAX / BATCH) + 1) * BATCH);
  }));
}

{
  const readable = new Readable({
    objectMode: true,
    read: common.mustCall(function() {
      console.log('>> READ');
      fetchData((err, data) => {
        if (err) {
          this.destroy(err);
          return;
        }

        if (data.length === 0) {
          console.log('pushing null');
          this.push(null);
          return;
        }

        console.log('pushing');
        data.forEach((d) => this.push(d));
      });
    }, Math.floor(MAX / BATCH) + 2)
  });

  let i = 0;
  function fetchData(cb) {
    if (i > MAX) {
      setTimeout(cb, 10, null, []);
    } else {
      const array = [];
      const max = i + BATCH;
      for (; i < max; i++) {
        array.push(i);
      }
      setTimeout(cb, 10, null, array);
    }
  }

  readable.on('data', (data) => {
    console.log('data emitted', data);
  });

  readable.on('end', common.mustCall(() => {
    assert.strictEqual(i, (Math.floor(MAX / BATCH) + 1) * BATCH);
  }));
}

{
  const readable = new Readable({
    objectMode: true,
    read: common.mustCall(function() {
      console.log('>> READ');
      fetchData((err, data) => {
        if (err) {
          this.destroy(err);
          return;
        }

        console.log('pushing');
        data.forEach((d) => this.push(d));

        if (data[BATCH - 1] >= MAX) {
          console.log('pushing null');
          this.push(null);
        }
      });
    }, Math.floor(MAX / BATCH) + 1)
  });

  let i = 0;
  function fetchData(cb) {
    const array = [];
    const max = i + BATCH;
    for (; i < max; i++) {
      array.push(i);
    }
    setTimeout(cb, 10, null, array);
  }

  readable.on('data', (data) => {
    console.log('data emitted', data);
  });

  readable.on('end', common.mustCall(() => {
    assert.strictEqual(i, (Math.floor(MAX / BATCH) + 1) * BATCH);
  }));
}

{
  const readable = new Readable({
    objectMode: true,
    read: common.mustNotCall()
  });

  readable.on('data', common.mustNotCall());

  readable.push(null);

  let nextTickPassed = false;
  process.nextTick(() => {
    nextTickPassed = true;
  });

  readable.on('end', common.mustCall(() => {
    assert.strictEqual(nextTickPassed, false);
  }));
}

{
  const readable = new Readable({
    objectMode: true,
    read: common.mustCall()
  });

  readable.on('data', (data) => {
    console.log('data emitted', data);
  });

  readable.on('end', common.mustCall());

  setImmediate(() => {
    readable.push('aaa');
    readable.push(null);
  });
}
