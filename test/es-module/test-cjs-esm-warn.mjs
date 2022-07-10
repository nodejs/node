import { mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import path from 'node:path';
import { execPath } from 'node:process';

import spawn from './helper.spawnAsPromised.mjs';


const requiringCjsAsEsm = path.resolve(fixtures.path('/es-modules/cjs-esm.js'));
const requiringEsm = path.resolve(fixtures.path('/es-modules/cjs-esm-esm.js'));
const pjson = path.resolve(
  fixtures.path('/es-modules/package-type-module/package.json')
);

{
  const required = path.resolve(
    fixtures.path('/es-modules/package-type-module/cjs.js')
  );
  const basename = 'cjs.js';
  spawn(execPath, [requiringCjsAsEsm])
    .then(mustCall(({ code, signal, stderr }) => {
      assert.ok(
        stderr.replaceAll('\r', '').includes(
          `Error [ERR_REQUIRE_ESM]: require() of ES Module ${required} from ${requiringCjsAsEsm} not supported.\n`
        )
      );
      assert.ok(
        stderr.replaceAll('\r', '').includes(
          `Instead rename ${basename} to end in .cjs, change the requiring ` +
          'code to use dynamic import() which is available in all CommonJS ' +
          `modules, or change "type": "module" to "type": "commonjs" in ${pjson} to ` +
          'treat all .js files as CommonJS (using .mjs for all ES modules ' +
          'instead).\n'
        )
      );

      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
    }));
}

{
  const required = path.resolve(
    fixtures.path('/es-modules/package-type-module/esm.js')
  );
  const basename = 'esm.js';
  spawn(execPath, [requiringEsm])
    .then(mustCall(({ code, signal, stderr }) => {
      assert.ok(
        stderr.replace(/\r/g, '').includes(
          `Error [ERR_REQUIRE_ESM]: require() of ES Module ${required} from ${requiringEsm} not supported.\n`
        )
      );
      assert.ok(
        stderr.replace(/\r/g, '').includes(
          `Instead change the require of ${basename} in ${requiringEsm} to` +
          ' a dynamic import() which is available in all CommonJS modules.\n'
        )
      );

      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
    }));
}
