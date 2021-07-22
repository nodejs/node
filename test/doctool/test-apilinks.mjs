import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import tmpdir from '../common/tmpdir.js';

import assert from 'assert';
import { execFileSync } from 'child_process';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const script = fileURLToPath(
  new URL('../../tools/doc/apilinks.mjs', import.meta.url));
const apilinks = fixtures.path('apilinks');

tmpdir.refresh();

fs.readdirSync(apilinks).forEach((fixture) => {
  if (!fixture.endsWith('.js')) return;
  const input = path.join(apilinks, fixture);

  const expectedContent = fs.readFileSync(`${input}on`, 'utf8');
  const outputPath = path.join(tmpdir.path, `${fixture}on`);
  execFileSync(
    process.execPath,
    [script, outputPath, input],
    { encoding: 'utf-8' }
  );

  const expectedLinks = JSON.parse(expectedContent);
  const actualLinks = JSON.parse(fs.readFileSync(outputPath));

  for (const [k, v] of Object.entries(expectedLinks)) {
    assert.ok(k in actualLinks, `link not found: ${k}`);
    assert.ok(actualLinks[k].endsWith('/' + v),
              `link ${actualLinks[k]} expected to end with ${v}`);
    delete actualLinks[k];
  }

  assert.strictEqual(
    Object.keys(actualLinks).length, 0,
    `unexpected links returned ${JSON.stringify(actualLinks)}`
  );
});
