'use strict';

const common = require('../common');
const assert = require('assert');
const { Blob, File } = require('buffer');
const { inspect } = require('util');

{
  // ensure File extends Blob
  assert.deepStrictEqual(Object.getPrototypeOf(File.prototype), Blob.prototype);
}

{
  assert.throws(() => new File(), TypeError);
  assert.throws(() => new File([]), TypeError);
}

{
  const properties = ['name', 'lastModified'];

  for (const prop of properties) {
    const desc = Object.getOwnPropertyDescriptor(File.prototype, prop);
    assert.notStrictEqual(desc, undefined);
    // Ensure these properties are getters.
    assert.strictEqual(desc.get?.name, `get ${prop}`);
    assert.strictEqual(desc.set, undefined);
    assert.strictEqual(desc.enumerable, true);
    assert.strictEqual(desc.configurable, true);
  }
}

{
  const file = new File([], '');
  assert.strictEqual(file[Symbol.toStringTag], 'File');
  assert.strictEqual(File.prototype[Symbol.toStringTag], 'File');
}

{
  assert.throws(() => File.prototype.name, TypeError);
  assert.throws(() => File.prototype.lastModified, TypeError);
}

{
  const keys = Object.keys(File.prototype).sort();
  assert.deepStrictEqual(keys, ['lastModified', 'name']);
}

{
  const file = new File([], 'dummy.txt.exe');
  assert.strictEqual(file.name, 'dummy.txt.exe');
  assert.strictEqual(file.size, 0);
  assert.strictEqual(typeof file.lastModified, 'number');
  assert(file.lastModified <= Date.now());
}

{
  const emptyFile = new File([], 'empty.txt');
  const blob = new Blob(['hello world']);

  emptyFile.text.call(blob).then(common.mustCall((text) => {
    assert.strictEqual(text, 'hello world');
  }));
}

{
  const toPrimitive = {
    [Symbol.toPrimitive]() {
      return 'NaN';
    }
  };

  const invalidLastModified = [
    null,
    undefined,
    'string',
    false,
    toPrimitive,
  ];

  for (const lastModified of invalidLastModified) {
    const file = new File([], '', { lastModified });
    assert.strictEqual(file.lastModified, 0);
  }
}

{
  const toPrimitive = {
    [Symbol.toPrimitive]() {
      throw new TypeError('boom');
    }
  };

  const throwValues = [
    BigInt(3n),
    toPrimitive,
  ];

  for (const lastModified of throwValues) {
    assert.throws(() => new File([], '', { lastModified }), TypeError);
  }
}

{
  const valid = [
    {
      [Symbol.toPrimitive]() {
        return 10;
      }
    },
    new Number(10),
    10,
  ];

  for (const lastModified of valid) {
    assert.strictEqual(new File([], '', { lastModified }).lastModified, 10);
  }
}

{
  const file = new File([], '');
  assert(inspect(file).startsWith('File { size: 0, type: \'\', name: \'\', lastModified:'));
}
