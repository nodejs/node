'use strict';
const common = require('../common');
const stream = require('stream');

{
  const r = new stream.Readable({
    autoDestroy: true,
    read() {
      this.push('hello');
      this.push('world');
      this.push(null);
    },
    destroy: common.mustCall((err, cb) => cb())
  });

  r.resume();
  r.on('end', common.mustCall());
  r.on('close', common.mustCall());
}

{
  const w = new stream.Writable({
    autoDestroy: true,
    write(data, enc, cb) {
      cb(null);
    },
    destroy: common.mustCall((err, cb) => cb())
  });

  w.write('hello');
  w.write('world');
  w.end();

  w.on('finish', common.mustCall());
  w.on('close', common.mustCall());
}

{
  const t = new stream.Transform({
    autoDestroy: true,
    transform(data, enc, cb) {
      cb(null, data);
    },
    destroy: common.mustCall((err, cb) => cb())
  });

  t.write('hello');
  t.write('world');
  t.end();

  t.resume();
  t.on('end', common.mustCall());
  t.on('finish', common.mustCall());
  t.on('close', common.mustCall());
}
