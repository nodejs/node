'use strict';

const common = require('../common');
const { Writable } = require('stream');

{
  const w = new Writable({
    write: common.mustCall((chunk, encoding, cb) => {
      w.on('close', common.mustCall(() => {
        cb();
      }));
    })
  });

  w.end('asd');
  w.destroy();
  w.on('finish', common.mustNotCall());
}

{
  const w = new Writable({
    write: common.mustCall((chunk, encoding, cb) => {
      w.on('close', common.mustCall(() => {
        cb();
        w.end();
      }));
    })
  });

  w.on('finish', common.mustNotCall());
  w.write('asd');
  w.destroy();
}
