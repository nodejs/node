// Tests the impact on eager operations required for policies affecting
// general startup,  does not test lazy operations
'use strict';
const common = require('../common.js');

const configs = {
  n: [1024],
};

const options = {
  flags: ['--expose-internals'],
};

const bench = common.createBenchmark(main, configs, options);

function main(conf) {
  const hash = (str, algo) => {
    const hash = require('crypto').createHash(algo);
    return hash.update(str).digest('base64');
  };
  const resources = Object.fromEntries(
    // Simulate graph of 1k modules
    Array.from({ length: 1024 }, (_, i) => {
      return [`./_${i}`, {
        integrity: `sha256-${hash(`// ./_${i}`, 'sha256')}`,
        dependencies: Object.fromEntries(Array.from({
          // Average 3 deps per 4 modules
          length: Math.floor((i % 4) / 2),
        }, (_, ii) => {
          return [`_${ii}`, `./_${i - ii}`];
        })),
      }];
    }),
  );
  const json = JSON.parse(JSON.stringify({ resources }), (_, o) => {
    if (o && typeof o === 'object') {
      Reflect.setPrototypeOf(o, null);
      Object.freeze(o);
    }
    return o;
  });
  const { Manifest } = require('internal/policy/manifest');

  bench.start();

  for (let i = 0; i < conf.n; i++) {
    new Manifest(json, 'file://benchmark/policy-relative');
  }

  bench.end(conf.n);
}
