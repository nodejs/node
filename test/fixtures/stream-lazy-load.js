'use strict';

// test/common and assert can initialize stdio, which accesses stream.Duplex.
// Capture the load lists before importing either of them.
const stream = require('stream');
const initiallyLoaded = process.moduleLoadList.slice();
Object.keys(stream);
Object.getOwnPropertyNames(stream.Readable.prototype);
const { Readable, Writable } = stream;
const afterEnumeration = process.moduleLoadList.slice();

const assert = require('assert');
let completed = false;
process.on('exit', () => assert(completed));
run(process.argv[2]).then(() => {
  completed = true;
});

async function run(scenario) {
  const optional = [
    'internal/abort_controller',
    'internal/streams/compose',
    'internal/streams/duplex',
    'internal/streams/duplexpair',
    'internal/streams/operators',
    'internal/streams/passthrough',
    'internal/streams/pipeline',
    'internal/streams/transform',
    'stream/promises',
  ];
  const loaded = (id) => process.moduleLoadList.includes(`NativeModule ${id}`);
  for (const id of optional) {
    assert.strictEqual(initiallyLoaded.includes(`NativeModule ${id}`), false, id);
    assert.strictEqual(afterEnumeration.includes(`NativeModule ${id}`), false, id);
  }
  assert.strictEqual(Readable, require('internal/streams/readable'));
  assert.strictEqual(Writable, require('internal/streams/writable'));

  switch (scenario) {
    case 'constructors': {
      for (const [name, id] of [
        ['Duplex', 'duplex'],
        ['Transform', 'transform'],
        ['PassThrough', 'passthrough'],
        ['duplexPair', 'duplexpair'],
      ]) {
        const descriptor = Object.getOwnPropertyDescriptor(stream, name);
        const value = require(`internal/streams/${id}`);
        assert.deepStrictEqual(descriptor, {
          value, writable: true, enumerable: true, configurable: true,
        });
        assert.strictEqual(stream[name], value);
      }
      assert.strictEqual(loaded('internal/streams/operators'), false);
      assert.strictEqual(loaded('internal/streams/pipeline'), false);
      assert.strictEqual(loaded('stream/promises'), false);
      break;
    }
    case 'assignment': {
      const replacement = () => {};
      stream.Transform = replacement;
      stream.Readable.prototype.map = replacement;
      assert.strictEqual(stream.Transform, replacement);
      assert.strictEqual(stream.Readable.prototype.map, replacement);
      assert.strictEqual(loaded('internal/streams/transform'), false);
      assert.strictEqual(loaded('internal/streams/operators'), false);
      break;
    }
    case 'operators': {
      const source = stream.Readable.from([1, 2, 3]);
      const map = source.map;
      assert.strictEqual(Object.hasOwn(source, 'map'), false);
      assert.strictEqual(map, stream.Readable.prototype.map);
      const expected = {
        drop: 1, filter: 2, flatMap: 2, map: 2, take: 1,
        every: 1, forEach: 2, reduce: 3, toArray: 1, some: 1, find: 2,
      };
      // Keep the lazy property list in sync with the implementation exports.
      assert.deepStrictEqual(Object.keys(require('internal/streams/operators')), Object.keys(expected));
      for (const [name, length] of Object.entries(expected)) {
        const fn = stream.Readable.prototype[name];
        assert.strictEqual(fn.name, name);
        assert.strictEqual(fn.length, length);
        assert.deepStrictEqual(Object.getOwnPropertyDescriptor(stream.Readable.prototype, name), {
          value: fn, enumerable: false, configurable: true, writable: true,
        });
        assert.throws(() => new fn(), { code: 'ERR_ILLEGAL_CONSTRUCTOR' });
      }
      assert.deepStrictEqual(await source.map((value) => value * 2).toArray(), [2, 4, 6]);
      assert.strictEqual(loaded('internal/streams/pipeline'), false);
      break;
    }
    case 'finished': {
      const { promisify } = require('util');
      const finished = promisify(stream.finished);
      assert.strictEqual(finished, stream.promises.finished);
      assert.strictEqual(stream.promises, require('stream/promises'));
      assert.strictEqual(loaded('internal/streams/pipeline'), false);
      await finished(stream.Readable.from([]).resume());
      assert.strictEqual(loaded('internal/streams/pipeline'), false);
      break;
    }
    case 'pipeline': {
      const callbackPipeline = stream.pipeline;
      assert.strictEqual(callbackPipeline, require('internal/streams/pipeline').pipeline);
      assert.strictEqual(loaded('stream/promises'), false);
      const { promisify } = require('util');
      assert.strictEqual(promisify(callbackPipeline), stream.promises.pipeline);
      const values = [];
      await stream.promises.pipeline(stream.Readable.from([1, 2, 3]), new stream.Writable({
        objectMode: true,
        write(chunk, encoding, cb) { values.push(chunk); cb(); },
      }));
      assert.deepStrictEqual(values, [1, 2, 3]);
      break;
    }
    case 'compose': {
      assert.strictEqual(stream.compose, require('internal/streams/compose'));
      const values = await stream.compose([1, 2], async function* (source) {
        for await (const item of source) yield item * 3;
      }).toArray();
      assert.deepStrictEqual(values, [3, 6]);
      break;
    }
    case 'esm': {
      const esm = await import('node:stream');
      assert.strictEqual(esm.default, stream);
      for (const name of [
        'Readable', 'Writable', 'Duplex', 'Transform', 'PassThrough',
        'pipeline', 'compose', 'promises',
      ]) {
        assert.strictEqual(esm[name], stream[name], name);
      }
      assert.strictEqual(loaded('internal/streams/operators'), false);
      function replacement() {}
      stream.Transform = replacement;
      require('module').syncBuiltinESMExports();
      assert.strictEqual(esm.Transform, replacement);
      break;
    }
    case 'vm': {
      const vm = require('vm');
      const transform = vm.runInNewContext('stream.Transform', { stream });
      assert.strictEqual(transform, stream.Transform);
      const map = vm.runInNewContext('stream.Readable.prototype.map', { stream });
      assert.strictEqual(map, stream.Readable.prototype.map);
      break;
    }
    default:
      assert.fail(`Unknown scenario: ${scenario}`);
  }
}
