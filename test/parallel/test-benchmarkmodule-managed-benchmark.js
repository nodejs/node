'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { Suite } = require('node:benchmark');
const { setTimeout } = require('timers/promises');

(async () => {
  {
    const suite = new Suite();
    const results = await suite.run();

    assert.ok(Array.isArray(results));
    assert.strictEqual(results.length, 0);
  }

  {
    const suite = new Suite();

    suite.add('wait 10ms', async () => {
      await setTimeout(10);
    });

    const results = await suite.run();

    assert.ok(Array.isArray(results));
    assert.strictEqual(results.length, 1);

    const firstResult = results[0];

    assert.strictEqual(typeof firstResult.opsSec, 'number');
    assert.strictEqual(typeof firstResult.iterations, 'number');
    assert.strictEqual(typeof firstResult.histogram, 'object');
    assert.strictEqual(typeof firstResult.histogram.cv, 'number');
    assert.strictEqual(typeof firstResult.histogram.min, 'number');
    assert.strictEqual(typeof firstResult.histogram.max, 'number');
    assert.strictEqual(typeof firstResult.histogram.mean, 'number');
    assert.strictEqual(typeof firstResult.histogram.percentile, 'function');
    assert.strictEqual(typeof firstResult.histogram.percentile(1), 'number');
    assert.strictEqual(firstResult.histogram.percentile(1), firstResult.histogram.samples[0]);
    assert.strictEqual(
      firstResult.histogram.percentile(100),
      firstResult.histogram.samples[firstResult.histogram.samples.length - 1],
    );
    assert.ok(Array.isArray(firstResult.histogram.samples));

    assert.ok(firstResult.histogram.samples.length >= 1, `received ${firstResult.histogram.samples.length}`);
    assert.ok(firstResult.opsSec >= 1, `received ${firstResult.opsSec}`);
  }

  {
    const suite = new Suite();

    suite.add('async empty', async () => { });
    suite.add('sync empty', () => { });

    const results = await suite.run();

    assert.strictEqual(results.length, 2);
    assert.ok(results[0].opsSec > 0);
    assert.ok(results[1].opsSec > 0);
  }

  {
    const suite = new Suite();

    suite.add('async empty', async (timer) => {
      timer.start();
      timer.end();
    });
    suite.add('sync empty', (timer) => {
      timer.start();
      timer.end();
    });

    const results = await suite.run();

    assert.strictEqual(results.length, 2);
    assert.ok(results[0].opsSec > 0);
    assert.ok(results[1].opsSec > 0);
  }

  {
    const suite = new Suite();

    suite.add('empty', (timer) => { });

    await assert.rejects(async () => {
      return await suite.run();
    }, common.expectsError({
      code: 'ERR_BENCHMARK_MISSING_OPERATION',
      message: 'You forgot to call .start()',
    }));
  }

  {
    const suite = new Suite();

    suite.add('async empty', async (timer) => {
      timer.start();
    });

    await assert.rejects(async () => {
      return await suite.run();
    }, common.expectsError({
      code: 'ERR_BENCHMARK_MISSING_OPERATION',
      message: 'You forgot to call .end(count)',
    }));
  }

  {
    const suite = new Suite();

    suite.add('empty', (timer) => {
      timer.end();
    });

    await assert.rejects(async () => {
      return await suite.run();
    }, common.expectsError({
      code: 'ERR_BENCHMARK_MISSING_OPERATION',
      message: 'You forgot to call .start()',
    }));
  }

  {
    const suite = new Suite();

    suite.add('empty', (timer) => {
      timer.end('error');
    });

    await assert.rejects(async () => {
      return await suite.run();
    }, common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "iterations" argument must be of type number. Received type string (\'error\')',
    }));
  }

  {
    const suite = new Suite();

    suite.add('empty', (timer) => {
      timer.end(-1);
    });

    await assert.rejects(async () => {
      return await suite.run();
    }, common.expectsError({
      code: 'ERR_OUT_OF_RANGE',
      message: 'The value of "iterations" is out of range. It must be >= 1 && <= 9007199254740991. Received -1',
    }));
  }

  {
    const { status, stdout, stderr } = spawnSync(
      process.execPath,
      [
        '-e', `
        const { Suite } = require('node:benchmark');
        const { setTimeout } = require('timers/promises');

        (async () => {
          const suite = new Suite();

          suite.add('wait 10ms', async () => {
            await setTimeout(10);
          });

          const results = await suite.run();
        })()`,
      ],
    );

    assert.strictEqual(status, 0, stderr.toString());

    const stdoutString = stdout.toString();
    assert.ok(stdoutString.includes('wait 10ms'), stdoutString);
    assert.ok(stdoutString.includes('ops/sec'), stdoutString);
    assert.ok(stdoutString.includes('+/-'), stdoutString);
    assert.ok(stdoutString.includes('min..max'), stdoutString);
    assert.ok(stdoutString.includes('runs sampled'), stdoutString);
    assert.ok(stdoutString.includes('p75'), stdoutString);
    assert.ok(stdoutString.includes('p99'), stdoutString);
  }
})().then(common.mustCall());

{
  const suite = new Suite();

  assert.throws(() => {
    suite.add();
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "name" argument must be of type string. Received undefined',
  }));

  assert.throws(() => {
    suite.add('test');
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "fn" argument must be of type function. Received undefined',
  }));

  assert.throws(() => {
    suite.add('test', () => { }, 123);
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "options" argument must be of type object. Received type number (123)',
  }));

  assert.throws(() => {
    suite.add('empty fn', () => { }, {
      minTime: 10,
      maxTime: 1,
    });
  }, common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    message: 'The value of "options.maxTime" is out of range. It must be >= 10. Received 1',
  }));

  assert.throws(() => {
    suite.add('empty fn', {
      minTime: 10,
      maxTime: 1,
    }, () => { });
  }, common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    message: 'The value of "options.maxTime" is out of range. It must be >= 10. Received 1',
  }));

  assert.throws(() => {
    suite.add('empty fn', {
      minTime: 1e9,
    }, () => { });
  }, common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    message: 'The value of "options.maxTime" is out of range. It must be >= 1000000000. Received 0.5',
  }));

  assert.throws(() => {
    suite.add('empty fn', {
      minTime: '1e9',
    }, () => { });
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "options.minTime" property must be of type number. Received type string (\'1e9\')',
  }));

  assert.throws(() => {
    suite.add('empty fn', {
      maxTime: '1e9',
    }, () => { });
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "options.maxTime" property must be of type number. Received type string (\'1e9\')',
  }));
}

{
  assert.throws(
    () => new Suite(1),
    common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options" argument must be of type object. Received type number (1)'
    }),
  );
  assert.throws(
    () => new Suite({ reporter: 'non-function' }),
    common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.reporter" property must be of type function. Received type string (\'non-function\')',
    }),
  );
  assert.throws(
    () => new Suite({ reporter: {} }),
    common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.reporter" property must be of type function. Received an instance of Object',
    }),
  );
}
