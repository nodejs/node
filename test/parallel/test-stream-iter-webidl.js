// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { Writable } = require('stream');
const {
  Broadcast,
  broadcast,
  bytesSync,
  from,
  fromWritable,
  fromSync,
  pipeTo,
  pull,
  push,
  share,
  shareSync,
  text,
  textSync,
} = require('stream/iter');

function testDictionaryAndIntegerConversion() {
  const pushed = push(null);
  assert.strictEqual(pushed.writer.endSync(), 0);
  const transformedWithNull = push((chunks) => chunks, null);
  transformedWithNull.writer.endSync();

  const broadcasted = broadcast(null);
  assert.strictEqual(broadcasted.writer.endSync(), 0);

  share(from(''), null).cancel();
  shareSync(fromSync(''), null).cancel();

  assert.deepStrictEqual(bytesSync(fromSync('data'), null),
                         new TextEncoder().encode('data'));
  assert.deepStrictEqual(
    bytesSync(fromSync('data'), { limit: '4.9' }),
    new TextEncoder().encode('data'),
  );
  assert.throws(
    () => bytesSync(fromSync('data'), { limit: -1 }),
    { name: 'TypeError', code: 'ERR_OUT_OF_RANGE' },
  );

  const converted = push({
    budget: '16384.9',
    backpressure: { toString: () => 'strict' },
  });
  assert.strictEqual(converted.writer.canWrite, true);
  converted.writer.endSync();

  let signalReads = 0;
  const transformed = push((chunks) => chunks, {
    get signal() {
      signalReads++;
      return undefined;
    },
  });
  assert.strictEqual(signalReads, 1);
  transformed.writer.endSync();
}

async function testUnknownDictionaryMembers() {
  const source = pull(from('pull'), {
    transform: 1,
    write: 1,
    unknown: true,
  });
  assert.strictEqual(await text(source), 'pull');

  let ended = false;
  const writer = {
    write() {},
    end() { ended = true; },
  };
  await pipeTo(from('pipe'), writer, {
    transform: 1,
    write: 1,
  });
  assert.strictEqual(ended, true);

  const options = {
    get encoding() {
      throw new Error('unknown member was read');
    },
  };
  assert.deepStrictEqual(bytesSync(fromSync('bytes'), options),
                         new TextEncoder().encode('bytes'));
  assert.strictEqual(textSync(fromSync('text'), {
    encoding: { toString: () => 'utf-8' },
  }), 'text');
}

async function testWriterConversion() {
  const { writer, readable } = push();

  await writer.write(42, null);
  assert.strictEqual(writer.writevSync(new Set([
    null,
    true,
    { toString: () => 'object' },
  ])), true);

  assert.throws(
    () => writer.write(Symbol('invalid')),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => writer.writev('not a sequence object'),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => writer.write('invalid options', 1),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  writer.endSync();
  assert.strictEqual(await text(readable), '42nulltrueobject');
}

async function testOtherWriterConversions() {
  const output = [];
  const writable = new Writable({
    write(chunk, encoding, callback) {
      output.push(chunk.toString());
      callback();
    },
  });
  const classicWriter = fromWritable(writable);
  await classicWriter.write(42, null);
  await classicWriter.writev(new Set([false, '!']));
  await classicWriter.end(null);
  assert.strictEqual(output.join(''), '42false!');

  const result = broadcast();
  const source = result.broadcast.push();
  await result.writer.write(42, null);
  await result.writer.writev(new Set([true]));
  result.writer.endSync();
  assert.strictEqual(await text(source), '42true');

  let signalReads = 0;
  const fromResult = Broadcast.from(from(''), {
    get signal() {
      signalReads++;
      return undefined;
    },
  });
  assert.strictEqual(signalReads, 1);
  fromResult.broadcast.cancel();
}

Promise.all([
  testDictionaryAndIntegerConversion(),
  testUnknownDictionaryMembers(),
  testWriterConversion(),
  testOtherWriterConversions(),
]).then(common.mustCall());
