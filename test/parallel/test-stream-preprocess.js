'use strict';
const common = require('../common');
const assert = require('assert');

const fs = require('fs');
const path = require('path');
const rl = require('readline');

const BOM = '\uFEFF';

// Get the data using a non-stream way to compare with the streamed data.
const modelData = fs.readFileSync(
  path.join(common.fixturesDir, 'file-to-read-without-bom.txt'), 'utf8'
);
const modelDataFirstCharacter = modelData[0];

// Detect the number of forthcoming 'line' events for mustCall() 'expected' arg.
const lineCount = modelData.match(/\n/g).length;

// Ensure both without-bom and with-bom test files are textwise equal.
assert.strictEqual(
  fs.readFileSync(
    path.join(common.fixturesDir, 'file-to-read-with-bom.txt'), 'utf8'
  ),
  `${BOM}${modelData}`
);

// An unjustified BOM stripping with a non-BOM character unshifted to a stream.
const inputWithoutBOM = fs.createReadStream(
  path.join(common.fixturesDir, 'file-to-read-without-bom.txt'), 'utf8'
);

inputWithoutBOM.once('readable', common.mustCall(() => {
  const maybeBOM = inputWithoutBOM.read(1);
  assert.strictEqual(maybeBOM, modelDataFirstCharacter);
  assert.notStrictEqual(maybeBOM, BOM);

  inputWithoutBOM.unshift(maybeBOM);

  let streamedData = '';
  rl.createInterface({
    input: inputWithoutBOM,
  }).on('line', common.mustCall((line) => {
    streamedData += `${line}\n`;
  }, lineCount)).on('close', common.mustCall(() => {
    assert.strictEqual(streamedData, modelData);
  }));
}));

// A justified BOM stripping.
const inputWithBOM = fs.createReadStream(
  path.join(common.fixturesDir, 'file-to-read-with-bom.txt'), 'utf8'
);

inputWithBOM.once('readable', common.mustCall(() => {
  const maybeBOM = inputWithBOM.read(1);
  assert.strictEqual(maybeBOM, BOM);

  let streamedData = '';
  rl.createInterface({
    input: inputWithBOM,
  }).on('line', common.mustCall((line) => {
    streamedData += `${line}\n`;
  }, lineCount)).on('close', common.mustCall(() => {
    assert.strictEqual(streamedData, modelData);
  }));
}));
