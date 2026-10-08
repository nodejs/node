import * as common from '../common/index.mjs';

import assert from 'node:assert';
import fs from 'node:fs/promises';
import path from 'node:path';

// This tests that `make doc` generates HTML and JSON.
// Note that for this test to pass, `make doc` must be run first.

if (common.isWindows) {
  common.skip('`make doc` does not run on Windows');
}

// Not all files under `doc/api` are expected in the generated output.
// Parse out list of excluded files.
let excludes = [];
const makefile = await fs.open(new URL('../../Makefile', import.meta.url));
try {
  // Find the `skip_apidoc_files = ` line and parse out the list of files.
  for await (const line of makefile.readLines()) {
    if (line.startsWith('skip_apidoc_files')) {
      excludes = line.split('=')[1]?.trim().split(/\s/).map((n) => path.basename(n));
    }
  }
} finally {
  makefile.close();
}

const outdir = new URL('../../out/doc/api', import.meta.url);
const files = await fs.readdir(outdir);

for await (const file of await fs.opendir(new URL('../../doc/api/', import.meta.url))) {
  // Only expect markdown files in doc/api.
  assert(path.extname(file.name), 'md');

  // Always expect markdown file in out/doc/api.
  assert(files.includes(file.name), `${file.name} not found (checked ${files})`);

  const htmlFile = `${path.basename(file.name, '.md')}.html`;
  const jsonFile = `${path.basename(file.name, '.md')}.json`;
  // Excluded files should not generate HTML and json files.
  if (excludes.includes(file.name)) {
    assert(!files.includes(htmlFile), `${htmlFile} was unexpectedly generated (checked ${files})`);
    assert(!files.includes(jsonFile), `${jsonFile} was unexpectedly generated (checked ${files})`);
  } else {
    assert(files.includes(htmlFile), `${htmlFile} was not generated (checked ${files})`);
    assert(files.includes(jsonFile), `${jsonFile} was not generated (checked ${files})`);
  }
}
