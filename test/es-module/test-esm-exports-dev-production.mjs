// Flags: --experimental-wasm-modules
import '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { strictEqual } from 'assert';
import { spawnSync } from 'child_process';

{
  const output = spawnSync(process.execPath, [path('/pkgexports-dev.mjs')], {
    env: Object.assign({}, process.env, {
      NODE_ENV: 'production'
    })
  });
  strictEqual(output.stdout.toString().trim(), 'production');
}

{
  const output = spawnSync(process.execPath, [path('/pkgexports-dev.mjs')], {
  });
  strictEqual(output.stdout.toString().trim(), 'development');
}

{
  const output = spawnSync(process.execPath, [path('/pkgexports-dev.mjs')], {
    NODE_ENV: 'any'
  });
  strictEqual(output.stdout.toString().trim(), 'development');
}
