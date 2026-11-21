'use strict';
const common = require('../common');
const assert = require('assert');
const { constants, createDeflateRaw, inflateRawSync } = require('zlib');

const input = Buffer.allocUnsafe(56489);
for (let i = 0; i < input.length; i++) {
  input[i] = (Math.imul(i, 2654435761) >>> 8) & 0xff;
}

{
  const output = [];
  const stream = createDeflateRaw({ level: 2, windowBits: 13 });

  stream.on('data', (chunk) => output.push(chunk));
  stream.on('end', common.mustCall(() => {
    assert.deepStrictEqual(inflateRawSync(Buffer.concat(output)), input);
  }));
  stream.params(1, constants.Z_FILTERED, common.mustCall());
  stream.end(input);
}

{
  const stream = createDeflateRaw();

  stream.on('data', () => {});
  stream.on('error', common.mustNotCall());
  stream.write(input, common.mustCall());
  assert.throws(() => stream._handle.params(1, constants.Z_FILTERED), {
    message: 'Cannot change zlib params while a write is in progress',
  });
  stream.end();
}

{
  // A parameter change is applied, and its callback called, even when no
  // other write follows it.
  const input = Buffer.from('a'.repeat(4096));
  const output = [];
  const stream = createDeflateRaw({ level: 6 });

  stream.on('data', (chunk) => output.push(chunk));
  stream.write(input);
  stream.params(1, constants.Z_FILTERED, common.mustSucceed(() => {
    assert.strictEqual(stream._level, 1);
    assert.strictEqual(stream._strategy, constants.Z_FILTERED);
    stream.end(input);
  }));
  stream.on('end', common.mustCall(() => {
    assert.deepStrictEqual(inflateRawSync(Buffer.concat(output)),
                           Buffer.concat([input, input]));
  }));
}

{
  // A flush, a parameter change and a following write queued together are
  // processed in order.
  const first = Buffer.from('first '.repeat(2000));
  const second = Buffer.from('second '.repeat(2000));
  const output = [];
  const stream = createDeflateRaw({ level: 9 });
  const order = [];

  stream.on('data', (chunk) => output.push(chunk));
  stream.write(first);
  stream.flush(common.mustCall(() => order.push('flush')));
  stream.params(1, constants.Z_DEFAULT_STRATEGY, common.mustCall(() => {
    order.push('params');
  }));
  stream.end(second);
  stream.on('end', common.mustCall(() => {
    assert.deepStrictEqual(order, ['flush', 'params']);
    assert.deepStrictEqual(inflateRawSync(Buffer.concat(output)),
                           Buffer.concat([first, second]));
  }));
}

{
  // Destroying a stream with a pending parameter change must not apply it
  // to a closed handle.
  const stream = createDeflateRaw();

  stream.on('data', () => {});
  stream.write(Buffer.allocUnsafe(1 << 20), common.mustCall());
  stream.params(1, constants.Z_FILTERED, common.mustCall((err) => {
    assert.ok(err);
  }));
  stream.destroy();
  stream.on('close', common.mustCall());
}

{
  // Consecutive parameter changes are all applied in order, even when the
  // last one goes back to the values the stream started with.
  const input = Buffer.from('b'.repeat(2048));
  const output = [];
  const stream = createDeflateRaw({ level: 6, strategy: constants.Z_DEFAULT_STRATEGY });
  const applied = [];

  stream.on('data', (chunk) => output.push(chunk));
  stream.write(input);
  stream.params(1, constants.Z_FILTERED, common.mustSucceed(() => {
    applied.push([stream._level, stream._strategy]);
  }));
  stream.params(6, constants.Z_DEFAULT_STRATEGY, common.mustSucceed(() => {
    applied.push([stream._level, stream._strategy]);
  }));
  stream.end(input);
  stream.on('end', common.mustCall(() => {
    assert.deepStrictEqual(applied, [
      [1, constants.Z_FILTERED],
      [6, constants.Z_DEFAULT_STRATEGY],
    ]);
    assert.deepStrictEqual(inflateRawSync(Buffer.concat(output)),
                           Buffer.concat([input, input]));
  }));
}
