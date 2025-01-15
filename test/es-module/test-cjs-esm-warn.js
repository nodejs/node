// Previously, this tested that require(esm) throws ERR_REQUIRE_ESM, which is no longer applicable
// since require(esm) is now supported. The test has been repurposed to ensure that the old behavior
// is preserved when the --no-experimental-require-module flag is used.
'use strict';

const { spawnPromisified } = require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('node:assert');
const path = require('node:path');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');

const requiringCjsAsEsm = path.resolve(fixtures.path('/es-modules/cjs-esm.js'));
const requiringEsm = path.resolve(fixtures.path('/es-modules/cjs-esm-esm.js'));
const pjson = path.toNamespacedPath(
  fixtures.path('/es-modules/package-type-module/package.json')
);

describe(
  'CJS ↔︎ ESM interop warnings',
  { concurrency: !process.env.TEST_PARALLEL },
  () => {
    it(async () => {
      const required = path.resolve(
        fixtures.path('/es-modules/package-type-module/cjs.js')
      );
      const basename = 'cjs.js';
      const { code, signal, stderr } = await spawnPromisified(execPath, [
        '--no-experimental-require-module',
        requiringCjsAsEsm,
      ]);

      assert.ok(
        stderr
          .replaceAll('\r', '')
          .includes(
            `Error [ERR_REQUIRE_ESM]: require() of ES Module ${required} from ${requiringCjsAsEsm} not supported.\n`
          )
      );
      assert.ok(
        stderr
          .replaceAll('\r', '')
          .includes(
            `Instead either rename ${basename} to end in .cjs, change the requiring ` +
              'code to use dynamic import() which is available in all CommonJS ' +
              `modules, or change "type": "module" to "type": "commonjs" in ${pjson} to ` +
              'treat all .js files as CommonJS (using .mjs for all ES modules ' +
              'instead).\n'
          )
      );

      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
    });

    it(async () => {
      const required = path.resolve(
        fixtures.path('/es-modules/package-type-module/esm.js')
      );
      const basename = 'esm.js';
      const { code, signal, stderr } = await spawnPromisified(execPath, [
        '--no-experimental-require-module',
        requiringEsm,
      ]);

      assert.ok(
        stderr
          .replace(/\r/g, '')
          .includes(
            `Error [ERR_REQUIRE_ESM]: require() of ES Module ${required} from ${requiringEsm} not supported.\n`
          )
      );
      assert.ok(
        stderr
          .replace(/\r/g, '')
          .includes(
            `Instead change the require of ${basename} in ${requiringEsm} to` +
              ' a dynamic import() which is available in all CommonJS modules.\n'
          )
      );

      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
    });
  }
);

describe(
  'CJS ↔︎ ESM interop warnings',
  { concurrency: !process.env.TEST_PARALLEL },
  () => {
    const codeErrorMessageForCJS =
      'The process should exit with status code 1 because import statements are not allowed in .cjs files.';
    const signalErrorMessageForCJS =
      'The process should not be terminated by a signal when "import" is used in a .cjs file.';
    const codeSuccessMessageForMJS =
      'The process should exit with status code 0 when loading a .mjs file in a CommonJS package.';
    const signalSuccessMessageForMJS =
      'The process should not be terminated by a signal when loading a .mjs file in a CommonJS package.';
    const outputSuccessMessageForMJS =
      'The output for loading a .mjs file in a CommonJS package should match the expected text.';

    // Test case 1: invalid-import-in-cjs.cjs
    it('should throw an error when "import" is used in a .cjs file', async () => {
      const filePath = path.resolve(
        fixtures.path('/es-modules/invalid-import-in-cjs.cjs')
      );
      const { code, signal, stderr } = await spawnPromisified(execPath, [
        filePath,
      ]);

      assert.ok(
        stderr
          .replace(/\r/g, '')
          .includes(
            `Cannot use 'import' statement in a .cjs file. CommonJS files (.cjs) do not support 'import'. Use 'require()' instead.`
          ),
        `Expected error message for "import" in .cjs file but got:\n${stderr}`
      );

      assert.strictEqual(code, 1, codeErrorMessageForCJS);
      assert.strictEqual(signal, null, signalErrorMessageForCJS);
    });

    // Test case 2: mjs-in-commonjs.mjs
    it('should load a .mjs file in a CommonJS package and display the correct output', async () => {
      const filePath = path.resolve(
        fixtures.path('/es-modules/mjs-in-commonjs.mjs')
      );
      const { code, signal, stdout } = await spawnPromisified(execPath, [
        filePath,
      ]);

      assert.ok(
        stdout
          .replace(/\r/g, '')
          .includes('This is a .mjs file in a CommonJS package.'),
        outputSuccessMessageForMJS
      );

      assert.strictEqual(code, 0, codeSuccessMessageForMJS);
      assert.strictEqual(signal, null, signalSuccessMessageForMJS);
    });
  }
);
