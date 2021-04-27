'use strict';

const common = require('../common');
const fs = require('fs');
const fsp = require('fs/promises');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const path = require('path');

tmpdir.refresh();

const content = '01'.repeat(5);
const sourceTemplate = path.resolve(tmpdir.path, 'source');
const destTemplate = path.resolve(tmpdir.path, 'dest');

// prepare fixture

async function createFixture(fixture) {
  const source = `${sourceTemplate}-${fixture}`;
  const dest = `${destTemplate}-${fixture}`;
  await fsp.writeFile(source, content);
  return [source, dest];
}

async function testStreamWithoutOption() {
  const [sourceName, destName] = await createFixture('no-options');
  const dest = fs.createWriteStream(destName);
  const handler = await fsp.open(sourceName);
  await handler.stream(dest);

  const destContent = await fsp.readFile(destName);
  assert.deepStrictEqual(destContent.toString(), content);
}

async function testStreamWithStart() {
  const [sourceName, destName] = await createFixture('with-start');
  const dest = fs.createWriteStream(destName);
  const handler = await fsp.open(sourceName);
  await handler.stream(dest, { start: 1 });
  const destContent = await fsp.readFile(destName);

  assert.deepStrictEqual(
    destContent.toString().length,
    content.slice(1).length,
    'length of copied text is not match with the origin'
  );
  assert.deepStrictEqual(
    destContent.toString(),
    content.slice(1),
    'content of copied text is not match with the origin'
  );
}

async function testStreamWithEnd() {
  const [sourceName, destName] = await createFixture('with-end');
  const dest = fs.createWriteStream(destName);
  const handler = await fsp.open(sourceName);
  await handler.stream(dest, { end: 1 });
  const destContent = await fsp.readFile(destName);

  assert.deepStrictEqual(
    destContent.toString().length,
    content.slice(0, 1).length,
    'length of copied text is not match with the origin'
  );
  assert.deepStrictEqual(
    destContent.toString(),
    content.slice(0, 1),
    'content of copied text is not match with the origin'
  );
}

async function testStreamWithStartAndEnd() {
  const [sourceName, destName] = await createFixture('with-start-and-end');
  const dest = fs.createWriteStream(destName);
  const handler = await fsp.open(sourceName);
  await handler.stream(dest, { start: 1, end: 5 });
  const destContent = await fsp.readFile(destName);

  assert.deepStrictEqual(
    destContent.toString().length,
    content.slice(1, 5).length,
    'length of copied text is not match with the origin'
  );
  assert.deepStrictEqual(
    destContent.toString(),
    content.slice(1, 5),
    'content of copied text is not match with the origin'
  );
}

async function testAbortController() {
  const [sourceName, destName] = await createFixture('abort-controller');
  const dest = fs.createWriteStream(destName);
  const handler = await fsp.open(sourceName);

  const abortController = new AbortController();
  process.nextTick(() => {
    abortController.abort();
  });

  assert.rejects(handler.stream(dest, { signal: abortController.signal }), {
    name: 'AbortError'
  });
}

async function testPrematureClose() {
  const [sourceName, destName] = await createFixture('premature-close');
  const dest = fs.createWriteStream(destName);
  const handler = await fsp.open(sourceName);

  dest.on('close', common.mustCall(() => {
    assert.rejects(handler.stream(dest), {
      code: 'ERR_STREAM_PREMATURE_CLOSE'
    });
  }));
  dest.destroy();
}


async function testArgumentValidation(options, expectedError) {
  const [sourceName, destName] = await createFixture('options');
  const dest = fs.createWriteStream(destName);
  const handler = await fsp.open(sourceName);

  assert.rejects(handler.stream(dest, options), expectedError);
}


(async () => {
  await testStreamWithoutOption();
  await testStreamWithStart();
  await testStreamWithEnd();
  await testStreamWithStartAndEnd();
  await testAbortController();
  await testPrematureClose();

  const negativeStart = [{
    start: -1
  }, {
    code: 'ERR_OUT_OF_RANGE',
    message: 'The value of "start" is out of range. ' +
      'It must be greater than 0. Received -1'
  }];

  const negativeEnd = [{
    end: -1
  }, {
    code: 'ERR_OUT_OF_RANGE',
    message: 'The value of "end" is out of range. ' +
      'It must be greater than 0. Received -1'
  }];

  const nonNumberStart = [{
    start: 'nonNumber'
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "start" argument must be of type number. ' +
      'Received type string (\'nonNumber\')'
  }];

  const nonNumberEnd = [{
    end: 'nonNumber'
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "end" argument must be of type number. ' +
      'Received type string (\'nonNumber\')'
  }];

  const startGreaterThanEnd = [{
    start: 4,
    end: 2
  }, {
    code: 'ERR_OUT_OF_RANGE',
    message: 'The value of "start" is out of range. ' +
      'It must be less than end 2. Received 4'
  }];
  await Promise.all(
    [
      negativeStart,
      negativeEnd,
      nonNumberStart,
      nonNumberEnd,
      startGreaterThanEnd,
    ].map((args) => testArgumentValidation(...args))
  );
})().then(common.mustCall());
