// Flags: --expose-internals
'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

if (Number(process.versions.openssl.split('.')[0]) < 4 ||
    process.features.openssl_is_boringssl) {
  common.skip('OpenSSL 4 provider support is required');
}

const assert = require('node:assert');
const {
  createHash,
  getHashes,
  hash,
} = require('node:crypto');
const { internalBinding } = require('internal/test/binding');
const {
  HashJob,
  kCryptoJobSync,
  kCryptoJobWebCrypto,
} = internalBinding('crypto');

const hashes = getHashes();
const hashNames = new Map(
  hashes.map((name) => [name.toLowerCase(), name]),
);

let exercised = false;

function findHash(...names) {
  for (const name of names) {
    const result = hashNames.get(name);
    if (result !== undefined) return result;
  }
  return undefined;
}

function testHashJob(args, expected) {
  const { 0: err, 1: result } = new HashJob(
    kCryptoJobSync,
    ...args,
  ).run();
  assert.strictEqual(err, undefined);
  assert.deepStrictEqual(Buffer.from(result), expected);

  (async () => {
    const asyncResult = await new HashJob(
      kCryptoJobWebCrypto,
      ...args,
    ).run();
    assert.deepStrictEqual(Buffer.from(asyncResult), expected);
  })().then(common.mustCall());
}

const cshakeVectors = [
  {
    names: ['cshake-128', 'cshake128'],
    shakeNames: ['shake128', 'shake-128'],
    outputLength: 32,
    input: Buffer.from('00010203', 'hex'),
    expected: 'c1c36925b6409a04f1b504fcbca9d82b' +
              '4017277cb5ed2b2065fc1d3814d5aaf5',
  },
  {
    names: ['cshake-256', 'cshake256'],
    shakeNames: ['shake256', 'shake-256'],
    outputLength: 64,
    input: Buffer.from('00010203', 'hex'),
    expected: 'd008828e2b80ac9d2218ffee1d070c48' +
              'b8e4c87bff32c9699d5b6896eee0edd1' +
              '64020e2be0560858d9c00c037e34a96' +
              '937c561a74c412bb4c746469527281c8c',
  },
];

for (const vector of cshakeVectors) {
  const algorithm = findHash(...vector.names);
  if (algorithm === undefined) {
    common.printSkipMessage(`${vector.names[0]} is not available`);
    continue;
  }

  exercised = true;

  const options = {
    outputLength: vector.outputLength,
    customization: 'Email Signature',
  };
  const streaming = createHash(algorithm, options)
    .update(vector.input.subarray(0, 2))
    .update(vector.input.subarray(2))
    .digest('hex');
  const partial = createHash(algorithm, options)
    .update(vector.input.subarray(0, 2));
  const copyOptionReads = [];
  const copied = partial.copy({
    get outputLength() {
      copyOptionReads.push('outputLength');
      return vector.outputLength;
    },
    get functionName() {
      copyOptionReads.push('functionName');
      return undefined;
    },
    get customization() {
      copyOptionReads.push('customization');
      return undefined;
    },
  })
    .update(vector.input.subarray(2))
    .digest('hex');

  assert.strictEqual(streaming, vector.expected);
  assert.strictEqual(copied, vector.expected);
  assert.deepStrictEqual(copyOptionReads, ['outputLength']);
  assert.strictEqual(hash(algorithm, vector.input, options), vector.expected);

  // BufferSource parameters have the same semantics as their string form.
  const bufferOptions = {
    ...options,
    customization: Buffer.from(options.customization),
  };
  assert.strictEqual(
    createHash(algorithm, bufferOptions).update(vector.input).digest('hex'),
    vector.expected,
  );
  assert.strictEqual(
    hash(algorithm, vector.input, bufferOptions),
    vector.expected,
  );

  // Without function-name and customization parameters, cSHAKE is SHAKE.
  const withoutParameters = createHash(algorithm)
    .update(vector.input)
    .digest('hex');
  assert.strictEqual(hash(algorithm, vector.input), withoutParameters);

  // Explicit undefined parameters have the same semantics as omitted ones.
  const undefinedOptions = {
    outputLength: vector.outputLength,
    functionName: undefined,
    customization: undefined,
  };
  assert.strictEqual(
    createHash(algorithm, undefinedOptions).update(vector.input).digest('hex'),
    withoutParameters,
  );
  assert.strictEqual(
    hash(algorithm, vector.input, undefinedOptions),
    withoutParameters,
  );

  // Empty BufferSource parameters are still supplied to OpenSSL, but cSHAKE
  // with two empty parameters is equivalent to SHAKE.
  const emptyOptions = {
    outputLength: vector.outputLength,
    functionName: Buffer.alloc(0),
    customization: new Uint8Array(0),
  };
  assert.strictEqual(
    createHash(algorithm, emptyOptions).update(vector.input).digest('hex'),
    withoutParameters,
  );
  assert.strictEqual(
    hash(algorithm, vector.input, emptyOptions),
    withoutParameters,
  );
  const emptyFunctionNameOptions = {
    outputLength: vector.outputLength,
    functionName: Buffer.alloc(0),
  };
  assert.strictEqual(
    createHash(algorithm, emptyFunctionNameOptions)
      .update(vector.input)
      .digest('hex'),
    withoutParameters,
  );
  assert.strictEqual(
    hash(algorithm, vector.input, emptyFunctionNameOptions),
    withoutParameters,
  );

  const createHashOptionReads = [];
  assert.strictEqual(
    createHash(algorithm, {
      get outputLength() {
        createHashOptionReads.push('outputLength');
        return vector.outputLength;
      },
      get functionName() {
        createHashOptionReads.push('functionName');
        return undefined;
      },
      get customization() {
        createHashOptionReads.push('customization');
        return undefined;
      },
    }).update(vector.input).digest('hex'),
    withoutParameters,
  );
  assert.deepStrictEqual(
    createHashOptionReads,
    ['outputLength', 'functionName', 'customization'],
  );

  const hashOptionReads = [];
  assert.strictEqual(
    hash(algorithm, vector.input, {
      get outputLength() {
        hashOptionReads.push('outputLength');
        return vector.outputLength;
      },
      get outputEncoding() {
        hashOptionReads.push('outputEncoding');
        return 'hex';
      },
      get functionName() {
        hashOptionReads.push('functionName');
        return undefined;
      },
      get customization() {
        hashOptionReads.push('customization');
        return undefined;
      },
    }),
    withoutParameters,
  );
  assert.deepStrictEqual(
    hashOptionReads,
    ['outputLength', 'outputEncoding', 'functionName', 'customization'],
  );

  for (const zeroLengthOptions of [
    { outputLength: 0 },
    {
      outputLength: 0,
      functionName: Buffer.alloc(0),
      customization: new Uint8Array(0),
    },
  ]) {
    assert.deepStrictEqual(
      createHash(algorithm, zeroLengthOptions).update(vector.input).digest(),
      Buffer.alloc(0),
    );
    assert.strictEqual(
      hash(algorithm, vector.input, zeroLengthOptions),
      '',
    );
  }

  const shake = findHash(...vector.shakeNames);
  if (shake !== undefined) {
    assert.strictEqual(
      withoutParameters,
      createHash(shake, { outputLength: vector.outputLength })
        .update(vector.input)
        .digest('hex'),
    );
  }

  const namedOptions = {
    outputLength: vector.outputLength,
    functionName: 'KMAC',
    customization: 'Node.js',
  };
  let namedResult;
  try {
    namedResult = createHash(algorithm, namedOptions)
      .update(vector.input)
      .digest();
  } catch {
    common.printSkipMessage(
      `${algorithm} does not support the KMAC function name`,
    );
  }
  if (namedResult !== undefined) {
    assert.deepStrictEqual(
      hash(algorithm, vector.input, {
        ...namedOptions,
        outputEncoding: 'buffer',
      }),
      namedResult,
    );
    assert.deepStrictEqual(
      createHash(algorithm, {
        ...namedOptions,
        functionName: Buffer.from(namedOptions.functionName),
        customization: new Uint8Array(Buffer.from(namedOptions.customization)),
      }).update(vector.input).digest(),
      namedResult,
    );
    assert.deepStrictEqual(
      hash(algorithm, vector.input, {
        ...namedOptions,
        functionName: Buffer.from(namedOptions.functionName),
        customization: new Uint8Array(Buffer.from(namedOptions.customization)),
        outputEncoding: 'buffer',
      }),
      namedResult,
    );
  }

  for (const functionName of ['', 'TupleHash', 'ParallelHash', 'KMAC']) {
    const functionOptions = {
      outputLength: vector.outputLength,
      functionName,
    };
    let functionResult;
    try {
      functionResult = createHash(algorithm, functionOptions)
        .update(vector.input)
        .digest();
    } catch {
      common.printSkipMessage(
        `${algorithm} does not support the ${functionName} function name`,
      );
      continue;
    }
    assert.deepStrictEqual(
      functionResult,
      hash(algorithm, vector.input, {
        ...functionOptions,
        outputEncoding: 'buffer',
      }),
    );
  }

  testHashJob([
    algorithm,
    vector.input,
    vector.outputLength * 8,
    undefined,
    Buffer.from(options.customization),
  ], Buffer.from(vector.expected, 'hex'));

  for (const invalidOptions of [
    { functionName: 1 },
    { customization: {} },
  ]) {
    const expected = { code: 'ERR_INVALID_ARG_TYPE' };
    assert.throws(() => createHash(algorithm, invalidOptions), expected);
    assert.throws(
      () => hash(algorithm, vector.input, invalidOptions),
      expected,
    );
  }

  for (const invalidOptions of [
    { functionName: 'KMAC\0' },
    { customization: 'Node\0js' },
    { customization: Buffer.from([0x61, 0x00, 0x62]) },
  ]) {
    const expected = { code: 'ERR_INVALID_ARG_VALUE' };
    assert.throws(() => createHash(algorithm, invalidOptions), expected);
    assert.throws(
      () => hash(algorithm, vector.input, invalidOptions),
      expected,
    );
  }
}

if (cshakeVectors.some(({ names }) => findHash(...names) !== undefined)) {
  for (const mismatchedOptions of [
    { functionName: 'KMAC' },
    { customization: 'Node.js' },
    { functionName: Buffer.alloc(0) },
    { customization: new Uint8Array(0) },
  ]) {
    assert.throws(
      () => createHash('sha256', mismatchedOptions),
      { message: 'Digest method not supported' },
    );
    assert.throws(
      () => hash('sha256', Buffer.from('abc'), mismatchedOptions),
      { message: 'Digest options are not supported' },
    );
  }
}

if (!exercised) {
  common.printSkipMessage('cSHAKE is not available');
}
