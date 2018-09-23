'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const check = Buffer.from([0x00, 0x01, 0x00, 0x00, 0x10, 0x00,
                           0x00, 0x05, 0x00, 0x00, 0x40, 0x00,
                           0x00, 0x04, 0x00, 0x00, 0xff, 0xff,
                           0x00, 0x06, 0x00, 0x00, 0xff, 0xff,
                           0x00, 0x02, 0x00, 0x00, 0x00, 0x01]);
const val = http2.getPackedSettings(http2.getDefaultSettings());
assert.deepStrictEqual(val, check);

[
  ['headerTableSize', 0],
  ['headerTableSize', 2 ** 32 - 1],
  ['initialWindowSize', 0],
  ['initialWindowSize', 2 ** 32 - 1],
  ['maxFrameSize', 16384],
  ['maxFrameSize', 2 ** 24 - 1],
  ['maxConcurrentStreams', 0],
  ['maxConcurrentStreams', 2 ** 31 - 1],
  ['maxHeaderListSize', 0],
  ['maxHeaderListSize', 2 ** 32 - 1]
].forEach((i) => {
  // Valid options should not throw.
  http2.getPackedSettings({ [i[0]]: i[1] });
});

http2.getPackedSettings({ enablePush: true });
http2.getPackedSettings({ enablePush: false });

[
  ['headerTableSize', -1],
  ['headerTableSize', 2 ** 32],
  ['initialWindowSize', -1],
  ['initialWindowSize', 2 ** 32],
  ['maxFrameSize', 16383],
  ['maxFrameSize', 2 ** 24],
  ['maxConcurrentStreams', -1],
  ['maxConcurrentStreams', 2 ** 31],
  ['maxHeaderListSize', -1],
  ['maxHeaderListSize', 2 ** 32]
].forEach((i) => {
  common.expectsError(() => {
    http2.getPackedSettings({ [i[0]]: i[1] });
  }, {
    code: 'ERR_HTTP2_INVALID_SETTING_VALUE',
    type: RangeError,
    message: `Invalid value for setting "${i[0]}": ${i[1]}`
  });
});

[
  1, null, '', Infinity, new Date(), {}, NaN, [false]
].forEach((i) => {
  common.expectsError(() => {
    http2.getPackedSettings({ enablePush: i });
  }, {
    code: 'ERR_HTTP2_INVALID_SETTING_VALUE',
    type: TypeError,
    message: `Invalid value for setting "enablePush": ${i}`
  });
});

{
  const check = Buffer.from([
    0x00, 0x01, 0x00, 0x00, 0x00, 0x64, 0x00, 0x03, 0x00, 0x00,
    0x00, 0xc8, 0x00, 0x05, 0x00, 0x00, 0x4e, 0x20, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x64, 0x00, 0x06, 0x00, 0x00, 0x00, 0x64,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x01]);

  const packed = http2.getPackedSettings({
    headerTableSize: 100,
    initialWindowSize: 100,
    maxFrameSize: 20000,
    maxConcurrentStreams: 200,
    maxHeaderListSize: 100,
    enablePush: true,
    foo: 'ignored'
  });
  assert.strictEqual(packed.length, 36);
  assert.deepStrictEqual(packed, check);
}

// Check for not passing settings.
{
  const packed = http2.getPackedSettings();
  assert.strictEqual(packed.length, 0);
}

{
  const packed = Buffer.from([
    0x00, 0x01, 0x00, 0x00, 0x00, 0x64, 0x00, 0x03, 0x00, 0x00,
    0x00, 0xc8, 0x00, 0x05, 0x00, 0x00, 0x4e, 0x20, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x64, 0x00, 0x06, 0x00, 0x00, 0x00, 0x64,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x01]);

  [1, true, '', [], {}, NaN].forEach((input) => {
    common.expectsError(() => {
      http2.getUnpackedSettings(input);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message:
        'The "buf" argument must be one of type Buffer, TypedArray, or ' +
        `DataView. Received type ${typeof input}`
    });
  });

  common.expectsError(() => {
    http2.getUnpackedSettings(packed.slice(5));
  }, {
    code: 'ERR_HTTP2_INVALID_PACKED_SETTINGS_LENGTH',
    type: RangeError,
    message: 'Packed settings length must be a multiple of six'
  });

  const settings = http2.getUnpackedSettings(packed);

  assert(settings);
  assert.strictEqual(settings.headerTableSize, 100);
  assert.strictEqual(settings.initialWindowSize, 100);
  assert.strictEqual(settings.maxFrameSize, 20000);
  assert.strictEqual(settings.maxConcurrentStreams, 200);
  assert.strictEqual(settings.maxHeaderListSize, 100);
  assert.strictEqual(settings.enablePush, true);
}

{
  const packed = Buffer.from([
    0x00, 0x02, 0x00, 0x00, 0x00, 0x00]);

  const settings = http2.getUnpackedSettings(packed, { validate: true });
  assert.strictEqual(settings.enablePush, false);
}
{
  const packed = Buffer.from([
    0x00, 0x02, 0x00, 0x00, 0x00, 0x64]);

  const settings = http2.getUnpackedSettings(packed, { validate: true });
  assert.strictEqual(settings.enablePush, true);
}

// Verify that passing {validate: true} does not throw.
{
  const packed = Buffer.from([
    0x00, 0x01, 0x00, 0x00, 0x00, 0x64, 0x00, 0x03, 0x00, 0x00,
    0x00, 0xc8, 0x00, 0x05, 0x00, 0x00, 0x4e, 0x20, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x64, 0x00, 0x06, 0x00, 0x00, 0x00, 0x64,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x01]);

  http2.getUnpackedSettings(packed, { validate: true });
}

// Check for maxFrameSize failing the max number.
{
  const packed = Buffer.from([0x00, 0x05, 0x01, 0x00, 0x00, 0x00]);

  common.expectsError(() => {
    http2.getUnpackedSettings(packed, { validate: true });
  }, {
    code: 'ERR_HTTP2_INVALID_SETTING_VALUE',
    type: RangeError,
    message: 'Invalid value for setting "maxFrameSize": 16777216'
  });
}

// Check for maxConcurrentStreams failing the max number.
{
  const packed = Buffer.from([0x00, 0x03, 0xFF, 0xFF, 0xFF, 0xFF]);

  common.expectsError(() => {
    http2.getUnpackedSettings(packed, { validate: true });
  }, {
    code: 'ERR_HTTP2_INVALID_SETTING_VALUE',
    type: RangeError,
    message: 'Invalid value for setting "maxConcurrentStreams": 4294967295'
  });
}
