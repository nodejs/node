// Flags: --enable-source-maps
'use strict';

const common = require('../common');
const assert = require('node:assert');
const childProcess = require('node:child_process');

// TODO(legendecas): c8 will complain if non-file-scheme is used in source maps.
// https://github.com/bcoe/c8/blob/ee2f1cfc5584d41bb2d51b788d0953dab0c798f8/lib/report.js#L517
if (process.env.NODE_V8_COVERAGE) {
  console.log('Spawning with coverage disabled.');
  const { status } = childProcess.spawnSync(
    process.execPath,
    [__filename],
    {
      stdio: 'inherit',
      env: {
        ...process.env,
        NODE_V8_COVERAGE: '',
      },
    },
  );
  assert.strictEqual(status, 0);
  return;
}

const sources = [
  `
  //# sourceMappingURL=unknown-protocol://invalid/url
  //# sourceURL=unknown-protocol://invalid/url
  `,
  `
  //# sourceMappingURL=file://invalid:/file/url
  //# sourceURL=file://invalid:/file/url
  `,
  `
  //# sourceMappingURL=invalid-url
  //# sourceURL=invalid-url
  `,
];

sources.forEach(test);

function test(source) {
  // Eval should tolerate invalid source map url
  eval(source);

  const base64 = Buffer.from(source).toString('base64url');

  // Dynamic import should tolerate invalid source map url
  import(`data:text/javascript;base64,${base64}`)
    .then(common.mustCall());
}

// eslint-disable-next-line @stylistic/js/spaced-comment
//# sourceMappingURL=invalid-url
