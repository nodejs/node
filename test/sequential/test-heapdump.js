'use strict';

const common = require('../common');

if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

const { writeHeapSnapshot, getHeapSnapshot } = require('v8');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
process.chdir(tmpdir.path);

{
  writeHeapSnapshot('my.heapdump');
  fs.accessSync('my.heapdump');
}

{
  const heapdump = writeHeapSnapshot();
  assert.strictEqual(typeof heapdump, 'string');
  fs.accessSync(heapdump);
}

[1, true, {}, [], null, Infinity, NaN].forEach((i) => {
  assert.throws(() => writeHeapSnapshot(i), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "path" argument must be of type string or an instance of ' +
             'Buffer or URL.' +
             common.invalidArgTypeHelper(i)
  });
});

{
  let data = '';
  const snapshot = getHeapSnapshot();
  snapshot.setEncoding('utf-8');
  snapshot.on('data', common.mustCallAtLeast((chunk) => {
    data += chunk.toString();
  }));
  snapshot.on('end', common.mustCall(() => {
    JSON.parse(data);
  }));
}
