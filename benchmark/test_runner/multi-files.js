'use strict';
const common = require('../common');

const bench = common.createBenchmark(main, {
  patterns: ['test/fixtures/test-runner/**/*.?(c|m)js', 'test/parallel/test-runner-*'],
  concurrency: ['yes', 'no'],
}, {
  flags: ['--expose-internals'],
});


function main({ patterns, concurrency }) {
  const { run } = require('node:test');
  const { Glob } = require('internal/fs/glob');
  const glob = new Glob([patterns]);
  const files = glob.globSync().filter((f) => !f.includes('never_ending') && !f.includes('watch-mode'));
  concurrency = concurrency === 'yes';
  let tests = 0;

  bench.start();
  (async function() {
    const stream = run({ concurrency, files });
    for await (const { type } of stream) {
      if (type === 'test:start') tests++;
    }
  })().then(() => bench.end(tests));
}
