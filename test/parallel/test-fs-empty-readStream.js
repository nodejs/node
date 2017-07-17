'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');

const emptyFile = fixtures.path('empty.txt');

fs.open(emptyFile, 'r', common.mustCall((error, fd) => {

  assert.ifError(error);

  const read = fs.createReadStream(emptyFile, { fd });

  read.once('data', () => {
    common.fail('data event should not emit');
  });

  read.once('end', common.mustCall(function endEvent1() {}));
}));

fs.open(emptyFile, 'r', common.mustCall((error, fd) => {

  assert.ifError(error);

  const read = fs.createReadStream(emptyFile, { fd });

  read.pause();

  read.once('data', () => {
    common.fail('data event should not emit');
  });

  read.once('end', function endEvent2() {
    common.fail('end event should not emit');
  });

  setTimeout(common.mustCall(() => {
    assert.strictEqual(read.isPaused(), true);
  }), common.platformTimeout(50));
}));
