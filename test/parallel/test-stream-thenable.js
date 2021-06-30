'use strict';
const common = require('../common');

const { Writable, Duplex, Readable } = require('stream');

{
  const w = new Writable({
    write(chunk, encoding, callback) {
      callback();
    }
  });

  (async function() {
    await w;
  }()).then(common.mustCall());

  w.end();
}


{
  const w = new Writable({
    write(chunk, encoding, callback) {
      callback();
    }
  });

  (async function() {
    await w;
  }()).then(common.mustNotCall(), common.mustCall());

  queueMicrotask(() => {
    w.destroy(new Error('asd'));
  });
}

{
  const d = new Duplex({
    write(chunk, encoding, callback) {
      callback();
    },
    read() {}
  });

  (async function() {
    await d;
  }()).then(common.mustCall());

  d.end('asd');
  d.push(null);
  d.resume();
}

{
  const d = new Duplex({
    write(chunk, encoding, callback) {
      callback();
    },
    read() {}
  });

  (async function() {
    await d;
  }()).then(common.mustNotCall(), common.mustCall());

  queueMicrotask(() => {
    d.destroy(new Error('asd'));
  });
}

{
  const r = new Readable({
    read() {}
  });

  (async function() {
    await r;
  }()).then(common.mustCall());

  r.push(null);
  r.resume();
}

{
  const r = new Readable({
    read() {}
  });

  (async function() {
    await r;
  }()).then(common.mustNotCall(), common.mustCall());

  queueMicrotask(() => {
    r.destroy(new Error('asd'));
  });
}
