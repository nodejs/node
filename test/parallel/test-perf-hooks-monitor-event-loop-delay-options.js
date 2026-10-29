'use strict';

// Tests the lowest, highest, and figures options of monitorEventLoopDelay().

const common = require('../common');
const assert = require('assert');
const { monitorEventLoopDelay } = require('perf_hooks');

const kMaxInt64 = 2n ** 63n - 1n;

function readHead(data, offset) {
  const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
  const info = data[offset] & 0x1f;
  const major = data[offset] >> 5;
  switch (info) {
    case 24: return { major, argument: BigInt(view.getUint8(offset + 1)), next: offset + 2 };
    case 25: return { major, argument: BigInt(view.getUint16(offset + 1)), next: offset + 3 };
    case 26: return { major, argument: BigInt(view.getUint32(offset + 1)), next: offset + 5 };
    case 27: return { major, argument: view.getBigUint64(offset + 1), next: offset + 9 };
    default:
      assert.ok(info < 24);
      return { major, argument: BigInt(info), next: offset + 1 };
  }
}

// Returns the histogram configuration from the documented export() format:
// a CBOR map in which key 1 is the lowest discernible value, key 2 the highest
// trackable value, key 3 the number of significant figures, and key 9 the
// length of the counts array.
function getLayout(histogram) {
  const data = histogram.export();
  const map = readHead(data, 0);
  assert.strictEqual(map.major, 5);
  const fields = new Map();
  let offset = map.next;
  for (let n = 0n; n < map.argument; n++) {
    const key = readHead(data, offset);
    const value = readHead(data, key.next);
    offset = value.next;
    if (value.major === 4) {
      // The counts array contains unsigned integers.
      for (let i = 0n; i < value.argument; i++)
        offset = readHead(data, offset).next;
    }
    fields.set(key.argument, value.argument);
  }
  return {
    lowest: fields.get(1n),
    highest: fields.get(2n),
    figures: fields.get(3n),
    countsLength: fields.get(9n),
  };
}

{
  // The defaults are unchanged.
  assert.deepStrictEqual(getLayout(monitorEventLoopDelay()), {
    lowest: 1000n,
    highest: kMaxInt64,
    figures: 3n,
    countsLength: 46080n,
  });
  assert.deepStrictEqual(
    getLayout(monitorEventLoopDelay({ samplePerIteration: true })), {
      lowest: 1n,
      highest: kMaxInt64,
      figures: 3n,
      countsLength: 55296n,
    });
}

for (const samplePerIteration of [false, true]) {
  assert.deepStrictEqual(getLayout(monitorEventLoopDelay({
    samplePerIteration,
    lowest: 1000,
    highest: 3_600_000_000_000,
    figures: 2,
  })), {
    lowest: 1000n,
    highest: 3_600_000_000_000n,
    figures: 2n,
    countsLength: 3456n,
  });

  assert.deepStrictEqual(getLayout(monitorEventLoopDelay({
    samplePerIteration,
    lowest: 1_000_000n,
    highest: 60_000_000_000n,
    figures: 1,
  })), {
    lowest: 1_000_000n,
    highest: 60_000_000_000n,
    figures: 1n,
    countsLength: 224n,
  });

  assert.deepStrictEqual(getLayout(monitorEventLoopDelay({
    samplePerIteration,
    lowest: 1_000_000,
  })), {
    lowest: 1_000_000n,
    highest: kMaxInt64,
    figures: 3n,
    countsLength: 35840n,
  });

  for (const name of ['lowest', 'highest', 'figures']) {
    for (const value of ['a', null, false, {}, []]) {
      assert.throws(() => monitorEventLoopDelay({
        samplePerIteration,
        [name]: value,
      }), { code: 'ERR_INVALID_ARG_TYPE' });
    }
  }
  assert.throws(() => monitorEventLoopDelay({ samplePerIteration, figures: 3n }),
                { code: 'ERR_INVALID_ARG_TYPE' });

  for (const options of [
    { lowest: 0 },
    { lowest: 1.5 },
    { lowest: 2 ** 53 },
    { lowest: 0n },
    { lowest: 2n ** 63n },
    { highest: 0 },
    { highest: 1.5 },
    { highest: 2 ** 53 },
    { highest: 2n ** 63n },
    { lowest: 10, highest: 19 },
    { lowest: 10n, highest: 19n },
    { figures: 0 },
    { figures: 6 },
    { figures: 1.5 },
  ]) {
    assert.throws(() => monitorEventLoopDelay({
      samplePerIteration,
      ...options,
    }), { code: 'ERR_OUT_OF_RANGE' });
  }

  // These options pass validation, but the histogram cannot be created.
  assert.throws(() => monitorEventLoopDelay({
    samplePerIteration,
    lowest: 2 ** 45,
    highest: 2 ** 46,
    figures: 5,
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

{
  // The default lowest depends on the sampling mode, and highest is validated
  // against it.
  assert.throws(() => monitorEventLoopDelay({ highest: 1999 }),
                { code: 'ERR_OUT_OF_RANGE' });
  monitorEventLoopDelay({ highest: 2000 });
  monitorEventLoopDelay({ samplePerIteration: true, highest: 1999 });
  assert.throws(() => monitorEventLoopDelay({
    samplePerIteration: true,
    highest: 1,
  }), { code: 'ERR_OUT_OF_RANGE' });
}

{
  // A right-sized histogram records one sample per event loop iteration.
  const iterations = 10;
  const histogram = monitorEventLoopDelay({
    samplePerIteration: true,
    lowest: 1000,
    highest: 3_600_000_000_000,
    figures: 2,
  });
  histogram.enable();

  const done = common.mustCall(() => {
    histogram.disable();
    assert.ok(histogram.count >= iterations - 1,
              `Expected at least ${iterations - 1} samples, got ${histogram.count}`);
    assert.strictEqual(histogram.exceeds, 0);
  });

  let remaining = iterations;
  function tick() {
    if (--remaining > 0) {
      setImmediate(tick);
    } else {
      done();
    }
  }
  setImmediate(tick);
}

{
  // Check that delays greater than highest are counted by exceeds.
  // Delayed timer callbacks can run close together, so some samples may
  // still be recorded despite resolution being much larger than highest.
  const histogram = monitorEventLoopDelay({
    resolution: 20,
    highest: 2_000_000,
  });
  histogram.enable();

  const done = common.mustCall(() => {
    histogram.disable();
    assert.ok(histogram.exceeds >= 2);
  });

  (function wait() {
    if (histogram.exceeds >= 2) {
      done();
    } else {
      setTimeout(wait, 5);
    }
  })();
}
