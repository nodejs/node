'use strict';

// A minimal decoder of pprof profiles, for the tests of node:perf_hooks/pprof.
//
// node:perf_hooks/pprof returns profiles as gzip-compressed protobuf
// messages, so tests can only check what a profile contains by decoding one.
// Core has no protobuf decoder, and the pprof code in lib/ only encodes.
// Decoding with code from the implementation would hide bugs that the encoder
// and the decoder share, and inspecting the Profile object before it is
// encoded would not test the bytes at all. This decoder is written
// independently of the encoder instead: it only knows the protobuf wire
// format and the field numbers of pprof's profile.proto, at
// https://github.com/google/pprof/blob/084b33a85f63291f0513dc6a6284e5dfa66c8e3a/proto/profile.proto
// It decodes the fields that the time profiler writes, and ignores the others.
//
// Tests usually pass the output of encode() or encodeSync() to
// decodeGzippedProfile(), and check the samples that it returns, which have
// their stacks resolved to function names, files and lines:
//
//   const { samples } = decodeGzippedProfile(encodeSync(time.stop()));
//   assert.ok(samples.some(({ stack }) => stack[0].name === 'busyLoop'));
//
// Tests of the encoding itself pass the uncompressed output of a Profile's
// encode() method to decodeProfile(), which returns the raw fields, with
// strings left as indices in the string table.

require('./');
const zlib = require('node:zlib');

/**
 * Reads a varint at `state.offset`, and advances the offset past it.
 * @param {Buffer} buffer The encoded message that holds the varint.
 * @param {{ offset: number }} state The offset of the varint in `buffer`,
 *   which is updated to the offset after it.
 * @returns {number | bigint} A number if the value is a safe integer, or a
 *   bigint otherwise.
 */
function readVarint(buffer, state) {
  let result = 0n;
  let shift = 0n;
  let byte;
  do {
    byte = buffer[state.offset++];
    result |= BigInt(byte & 0x7f) << shift;
    shift += 7n;
  } while (byte & 0x80);
  return result <= BigInt(Number.MAX_SAFE_INTEGER) ? Number(result) : result;
}

/**
 * Calls `onField(field, value)` for each field of a message, where `value` is
 * a number (or bigint) for varints and a Buffer for length-delimited fields.
 * @param {Buffer} buffer The encoded message.
 * @param {(field: number, value: number | bigint | Buffer) => void} onField
 *   Called with the field number and the value of each field, in order.
 * @throws {Error} If a field has a wire type other than varint and
 *   length-delimited, which pprof messages do not use.
 */
function decodeFields(buffer, onField) {
  const state = { offset: 0 };
  while (state.offset < buffer.length) {
    const tag = readVarint(buffer, state);
    const field = Math.floor(tag / 8);
    const wireType = tag % 8;
    if (wireType === 0) {
      onField(field, readVarint(buffer, state));
    } else if (wireType === 2) {
      const length = readVarint(buffer, state);
      onField(field, buffer.subarray(state.offset, state.offset + length));
      state.offset += length;
    } else {
      throw new Error(`Unexpected wire type ${wireType}`);
    }
  }
}

/**
 * Decodes the varints of a packed repeated field.
 * @param {Buffer} buffer The payload of the field, without its tag and
 *   length.
 * @returns {Array<number | bigint>} The elements of the field.
 */
function decodePacked(buffer) {
  const values = [];
  const state = { offset: 0 };
  while (state.offset < buffer.length) {
    values.push(readVarint(buffer, state));
  }
  return values;
}

/**
 * Decodes a message according to `schema`, which maps property names to
 * `{ field, repeated, packed, decode }`, where `field` is the field number and
 * `decode` optionally decodes the raw value. Repeated properties default to
 * `[]`, others to `0`, and unknown fields are ignored.
 * @param {Buffer} buffer The encoded message, without its tag and length.
 * @param {object} schema The properties to decode, each mapped to its
 *   `{ field, repeated, packed, decode }`.
 * @returns {object} The message, with one property per property of
 *   `schema`.
 * @throws {Error} If the message has a field with an unsupported wire type.
 */
function decodeMessage(buffer, schema) {
  const message = {};
  for (const [name, { repeated }] of Object.entries(schema)) {
    message[name] = repeated ? [] : 0;
  }
  const names = Object.keys(schema);
  decodeFields(buffer, (field, value) => {
    const name = names.find((n) => schema[n].field === field);
    if (name === undefined) return;
    const { decode, packed, repeated } = schema[name];
    if (packed) {
      // Repeated scalars may be packed or not.
      message[name].push(...(Buffer.isBuffer(value) ? decodePacked(value) : [value]));
    } else if (repeated) {
      message[name].push(decode ? decode(value) : value);
    } else {
      message[name] = decode ? decode(value) : value;
    }
  });
  return message;
}

const valueTypeSchema = {
  type: { field: 1 },
  unit: { field: 2 },
};
const lineSchema = {
  functionId: { field: 1 },
  line: { field: 2 },
  column: { field: 3 },
};
const profileSchema = {
  sampleType: {
    field: 1,
    repeated: true,
    decode: (b) => decodeMessage(b, valueTypeSchema),
  },
  sample: {
    field: 2,
    repeated: true,
    decode: (b) => decodeMessage(b, {
      locationId: { field: 1, packed: true, repeated: true },
      value: { field: 2, packed: true, repeated: true },
    }),
  },
  location: {
    field: 4,
    repeated: true,
    decode: (b) => decodeMessage(b, {
      id: { field: 1 },
      line: {
        field: 4,
        repeated: true,
        decode: (l) => decodeMessage(l, lineSchema),
      },
    }),
  },
  function: {
    field: 5,
    repeated: true,
    decode: (b) => decodeMessage(b, {
      id: { field: 1 },
      name: { field: 2 },
      systemName: { field: 3 },
      filename: { field: 4 },
    }),
  },
  stringTable: {
    field: 6,
    repeated: true,
    decode: (b) => b.toString('utf8'),
  },
  timeNanos: { field: 9 },
  durationNanos: { field: 10 },
  periodType: { field: 11, decode: (b) => decodeMessage(b, valueTypeSchema) },
  period: { field: 12 },
  comment: { field: 13, packed: true, repeated: true },
};

/**
 * Decodes an uncompressed pprof Profile message.
 * @param {Buffer} buffer The encoded Profile message.
 * @returns {object} The fields of the profile, with the property names of
 *   `profile.proto`, and messages decoded into objects.
 * @throws {Error} If the message has a field with an unsupported wire type.
 */
function decodeProfile(buffer) {
  return decodeMessage(buffer, profileSchema);
}

/**
 * Decodes a gzip-compressed pprof profile, and resolves its samples into
 * `{ stack, values }`, where `stack` lists the frames, leaf first, as
 * `{ name, file, line }`, and `values` maps sample types to values.
 * @param {Buffer} buffer The gzip-compressed encoding of a profile, as
 *   returned by `encode()` or `encodeSync()`.
 * @returns {{
 *   profile: object,
 *   strings: string[],
 *   sampleTypes: string[],
 *   samples: Array<{ stack: object[], values: object }>,
 * }} The decoded profile, its string table, the names of its sample types,
 *   and its resolved samples.
 * @throws {Error} If `buffer` is not gzip-compressed, or if the profile has a
 *   field with an unsupported wire type.
 */
function decodeGzippedProfile(buffer) {
  const profile = decodeProfile(zlib.gunzipSync(buffer));
  const strings = profile.stringTable;
  const functions = new Map(profile.function.map((f) => [f.id, f]));
  const locations = new Map(profile.location.map((l) => [l.id, l]));
  const sampleTypes = profile.sampleType.map((t) => strings[t.type]);
  const samples = profile.sample.map((sample) => ({
    stack: sample.locationId.map((id) => {
      const line = locations.get(id).line[0];
      const fn = functions.get(line.functionId);
      return {
        name: strings[fn.name],
        file: strings[fn.filename],
        line: line.line,
      };
    }),
    values: Object.fromEntries(
      sampleTypes.map((type, i) => [type, sample.value[i] ?? 0])),
  }));
  return { profile, strings, sampleTypes, samples };
}

/**
 * Keeps the thread busy until it has used `cpuMicros` microseconds of CPU
 * time, so that profiles attribute samples to this function. Tests look for
 * it in profiles by its name, `pprofTestBusyLoop`, and its file, this one.
 *
 * The loop is bounded by CPU time rather than wall time, so that it gets
 * about as many samples on a loaded machine as on an idle one. V8 takes a
 * sample by interrupting the thread, so a thread that the operating system
 * does not run gets no samples. A loop bounded by wall time would run for
 * less and less of that time as the load grows, and get fewer samples.
 *
 * The loop mostly runs plain JavaScript rather than builtins, because V8 can
 * attribute the samples taken in a builtin to its caller.
 *
 * Optimized code that was compiled before a profiler started lacks the source
 * positions that attribute samples to inlined functions, so tests that use
 * this function prevent its optimization, with `--allow-natives-syntax` and
 * `eval('%NeverOptimizeFunction(pprofTestBusyLoop)')`.
 * @param {number} cpuMicros The CPU time to use, in microseconds.
 * @returns {number} A value computed by the loop, so that it has an effect.
 */
function pprofTestBusyLoop(cpuMicros) {
  const start = process.threadCpuUsage();
  let x = 0;
  let used = 0;
  while (used < cpuMicros) {
    for (let i = 0; i < 100_000; i++) x = (x + i * i) % 1_000_003;
    const { user, system } = process.threadCpuUsage(start);
    used = user + system;
  }
  return x;
}

module.exports = {
  decodeGzippedProfile,
  decodeProfile,
  pprofTestBusyLoop,
};
