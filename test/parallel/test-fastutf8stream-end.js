// Flags: --expose-internals
'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const { it } = require('node:test');
const fs = require('fs');
const path = require('path');
const FastUtf8Stream = require('internal/streams/fast-utf8-stream');

tmpdir.refresh();
process.umask(0o000);

const files = [];
let count = 0;

function file() {
  const file = path.join(tmpdir.path,
                         `sonic-boom-${process.pid}-${process.hrtime().toString()}-${count++}`);
  files.push(file);
  return file;
}

it('end after reopen sync', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: true });

  stream.once('ready', common.mustCall(() => {
    const after = dest + '-moved';
    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, 'after reopen\n');
      }));
    }));
    stream.end();
  }));
});

it('end after reopen', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: false });

  stream.once('ready', common.mustCall(() => {
    const after = dest + '-moved';
    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, 'after reopen\n');
      }));
    }));
    stream.end();
  }));
});

it('end after 2x reopen sync', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: true });

  stream.once('ready', common.mustCall(() => {
    stream.reopen(dest + '-moved');
    const after = dest + '-moved-moved';
    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, 'after reopen\n');
      }));
    }));
    stream.end();
  }));
});

it('end after 2x reopen', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: false });

  stream.once('ready', common.mustCall(() => {
    stream.reopen(dest + '-moved');
    const after = dest + '-moved-moved';
    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, 'after reopen\n');
      }));
    }));
    stream.end();
  }));
});

it('end if not ready sync', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: true });
  const after = dest + '-moved';
  stream.reopen(after);
  stream.write('after reopen\n');
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'after reopen\n');
    }));
  }));
  stream.end();
});

it('end if not ready', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: false });
  const after = dest + '-moved';
  stream.reopen(after);
  stream.write('after reopen\n');
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'after reopen\n');
    }));
  }));
  stream.end();
});
