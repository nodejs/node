'use strict';

const common = require('../common');

const { setTimeout: wait } = require('node:timers/promises');
const assert = require('assert');
const {
  counter,
  gauge,
  meter,
  timer,
  uniqueSet,
  periodicGauge,

  statsdStream,
  dogstatsdStream,
  graphiteStream,
  prometheusStream,
} = require('node:metrics');

const statsd = statsdStream();
const dogstatsd = dogstatsdStream();
const graphite = graphiteStream();
const prometheus = prometheusStream();

async function assertStream(stream, expected) {
  // Mark stream as ended so the toArray() can resolve.
  stream.end();
  const chunks = await stream.toArray();
  const actual = Buffer.concat(chunks).toString().split('\n').filter((v) => v);
  assert.strictEqual(actual.length, expected.length, `Stream should yield ${expected.length} lines`);
  for (let i = 0; i < actual.length; i++) {
    if (typeof expected[i] === 'string') {
      assert.strictEqual(actual[i], expected[i], `Stream line ${i + 1} should match expected output`);
    } else {
      assert.ok(expected[i].test(actual[i]), `Stream line ${i + 1} should match expected output (${actual[i]})`);
    }
  }
}

async function main() {
  const c = counter('my-counter', { metaFor: 'my-counter' });
  c.increment(1, { more: 'meta' });

  const g = gauge('my-gauge', { metaFor: 'my-gauge' });
  g.reset(123, { more: 'meta' });

  const m = meter('my-meter', 100, { metaFor: 'my-meter' });
  m.mark(1, { more: 'meta' });

  const t = timer('my-timer', { metaFor: 'my-timer' });
  const t1 = t.create({ more: 't1' });
  const t2 = t.create({ more: 't2' });

  await wait(50);
  t1.stop();

  await wait(100);
  t2.stop();

  const s = uniqueSet('my-set', { metaFor: 'my-set' });
  s.reset(123, { more: 'meta' });

  await new Promise((resolve) => {
    const pg = periodicGauge('my-periodic-gauge', 50, () => {
      setImmediate(resolve);
      clearInterval(timer);
      pg.stop();
      return 100;
    }, {
      metaFor: 'my-periodic-gauge'
    });

    // Keep the loop alive
    pg.ref();
  });

  await Promise.all([
    assertStream(statsd, [
      'my-counter:1|c',
      'my-gauge:123|g',
      'my-meter:1|m',
      /^my-timer:\d+(\.\d+)?\|ms$/,
      /^my-timer:\d+(\.\d+)?\|ms$/,
      'my-set:123|s',
      'my-periodic-gauge:100|g',
    ]),
    assertStream(dogstatsd, [
      'my-counter:1|c|metaFor:my-counter,more:meta',
      'my-gauge:123|g|metaFor:my-gauge,more:meta',
      'my-meter:1|m|metaFor:my-meter,more:meta',
      /^my-timer:\d+(\.\d+)?\|ms\|metaFor:my-timer,more:t1$/,
      /^my-timer:\d+(\.\d+)?\|ms\|metaFor:my-timer,more:t2$/,
      'my-set:123|s|metaFor:my-set,more:meta',
      'my-periodic-gauge:100|g|metaFor:my-periodic-gauge',
    ]),
    assertStream(graphite, [
      'my-counter 1 0',
      'my-gauge 123 0',
      'my-meter 1 0',
      /^my-timer \d+(\.\d+)? 0$/,
      /^my-timer \d+(\.\d+)? 0$/,
      'my-set 123 0',
      'my-periodic-gauge 100 0',
    ]),
    assertStream(prometheus, [
      /^my-counter{metaFor="my-counter",more="meta"} 1 \d+(\.\d+)?$/,
      /^my-gauge{metaFor="my-gauge",more="meta"} 123 \d+(\.\d+)?$/,
      /^my-meter{metaFor="my-meter",more="meta"} 1 \d+(\.\d+)?$/,
      /^my-timer{metaFor="my-timer",more="t1"} \d+(\.\d+)? \d+(\.\d+)?$/,
      /^my-timer{metaFor="my-timer",more="t2"} \d+(\.\d+)? \d+(\.\d+)?$/,
      /^my-set{metaFor="my-set",more="meta"} 123 \d+(\.\d+)?$/,
      /^my-periodic-gauge{metaFor="my-periodic-gauge"} 100 \d+(\.\d+)?$/,
    ]),
  ]);
}

main().then(common.mustCall());
