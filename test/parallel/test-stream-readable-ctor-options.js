'use strict';
const common = require('../common');

const { Readable } = require('stream');
const { strictEqual } = require('assert');

{
  const _read = common.mustCall(function _read(n) {
    this.push(null);
  });

  const r = new Readable({ read: _read });
  r.resume();
}

{
  const { _readableState: state } = new Readable();
  strictEqual(state.objectMode, false);
  strictEqual(state.skipChunkCheck, false);
}

{
  const { _readableState: state } = new Readable({ objectMode: true });
  strictEqual(state.objectMode, true);
  strictEqual(state.skipChunkCheck, true);
}

{
  const { _readableState: state } = new Readable({ skipChunkCheck: true });
  strictEqual(state.objectMode, false);
  strictEqual(state.skipChunkCheck, true);
}
