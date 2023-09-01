'use strict';

const common = require('../common');

const { open, readFile } = require('node:fs/promises');
const { pipeline } = require('node:stream/promises');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;
const { it } = require('node:test');

tmpdir.refresh();

// TODO - add test to check if can write to file handle itself after it was closed
it('should allow writing from the 2nd write stream of file handle after the 1st stream destroyed', async () => {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write.txt');

  // TODO - allow closing the fd when signal is aborted
  const stream0Text = 'Hello world from stream0';
  const stream1Text = 'Hello world from stream1';

  const destFile = await open(filePathForHandle, 'w');
  await destFile.truncate(stream0Text.length + stream1Text.length + 20);

  const stream0 = destFile.createWriteStream({ start: 0, autoClose: false, emitClose: false });
  const stream1 = destFile.createWriteStream({ start: stream0Text.length + 10, autoClose: false, emitClose: false });

  await pipeline(stream0Text, stream0);
  await pipeline(stream1Text, stream1);

  stream0.destroy(new Error('destroyed'));
  stream1.destroy(new Error('destroyed'));

  const spaceBetween = '\x00'.repeat(10);
  const readFileData = (await readFile(filePathForHandle)).toString();
  assert.deepStrictEqual(`${stream0Text}${spaceBetween}${stream1Text}${spaceBetween}`, readFileData);
});

it('should close the file handle when no more streams referencing it', async () => {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write.txt');

  const stream0Text = 'Hello world from stream0';

  const destFile = await open(filePathForHandle, 'w');
  await destFile.truncate(stream0Text.length + 100);

  const stream0 = destFile.createWriteStream({ start: 0 });
  stream0.on('error', common.mustNotCall());
  await pipeline(stream0Text, stream0);

  assert.throws(() => {
    destFile.createWriteStream({ start: stream0Text.length + 10 });
  }, /ERR_OUT_OF_RANGE/);

  const readFileData = (await readFile(filePathForHandle)).toString();
  assert.deepStrictEqual(`${stream0Text}${'\x00'.repeat(100)}`, readFileData);
});
