'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const file = path.join(tmpdir.path, '/read_stream_filehandle_worker.txt');
const input = 'hello world';
const { Worker, isMainThread, workerData } = require('worker_threads');

let output = '';
tmpdir.refresh();
fs.writeFileSync(file, input);

if (isMainThread) {
  fs.promises.open(file, 'r').then((handle) => {
    handle.on('close', common.mustNotCall());
    new Worker(__filename, {
      workerData: { handle },
      transferList: [handle]
    });
  });
  fs.promises.open(file, 'r').then((handle) => {
    handle.on('close', common.mustNotCall());
    fs.createReadStream(null, { fd: handle });
    assert.throws(() => {
      new Worker(__filename, {
        workerData: { handle },
        transferList: [handle]
      });
    }, {
      code: 25,
    });
  });
} else {
  const handle = workerData.handle;
  const stream = fs.createReadStream(null, { fd: handle });

  stream.on('data', common.mustCallAtLeast((data) => {
    output += data;
  }));

  stream.on('end', common.mustCall(() => {
    assert.strictEqual(output, input);
  }));

  stream.on('close', common.mustCall());
}
