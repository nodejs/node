'use strict';

const common = require('../common');
const { Writable } = require('stream');

{
  const w = new Writable({
    write(chunk, encoding, cb) {
      w.on('close', () => {
        cb();
      });
    }
  });

  w.end('asd');
  w.destroy();
  w.on('finish', common.mustNotCall());
}

{
  const w = new Writable({
    write(chunk, encoding, cb) {
      w.on('close', () => {
        cb();
        w.end();
      });
    }
  });

  w.on('finish', common.mustNotCall());
  w.write('asd');
  w.destroy();
}
