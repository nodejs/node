import '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { strictEqual } from 'assert';
import { spawnSync } from 'child_process';

{
  const output = spawnSync(process.execPath, [path('/pkgexports-dev.mjs')]);
  console.log(output.stderr.toString());
  strictEqual(output.stdout.toString().trim(), 'production');
}

{
  const output = spawnSync(process.execPath,
                           ['--dev', path('/pkgexports-dev.mjs')]);
  strictEqual(output.stdout.toString().trim(), 'development');
}
