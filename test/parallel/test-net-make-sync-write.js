'use strict';
// Flags: --expose-internals
const common = require('../common');
const assert = require('assert');
const { makeSyncWrite } = require('internal/net');
{
  const _this = {
    _handle: {
      bytesWritten: 0,
    },
    write: makeSyncWrite(1),
  };
  _this.write(
    'chunk\n',
    null,
    common.mustCall(() => {
      assert.strictEqual(_this._handle.bytesWritten, 6);
    })
  );
}
