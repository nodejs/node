#!/usr/bin/env node

// Extracts C++ addon examples from doc/api/addons.md into numbered test
// directories under test/addons/.
//
// Each code block to extract is preceded by a marker in the markdown:
//
//   <!-- addon-verify-file worker_support/addon.cc [...] -->
//   ```cpp
//   #include <node.h>
//   ...
//   ```
//
// This produces test/addons/01_worker_support/addon.cc.
// Sections are numbered in order of first appearance.

import { mkdirSync, writeFileSync } from 'node:fs';
import { open } from 'node:fs/promises';
import { join } from 'node:path';
import { parseArgs } from 'node:util';

const { values } = parseArgs({
  options: {
    input: { type: 'string' },
    output: { type: 'string' },
  },
});

if (!values.input || !values.output) {
  console.error('Usage: addon-verify.mjs --input <file> --output <dir>');
  process.exit(1);
}

const src = await open(values.input, 'r');

const MARKER_RE = /^<!--\s*addon-verify-file\s+(?<filenames>((\S+?)\/(\S+)\s?)+)\s*-->$/;

const entries = [];
let nextBlockIsAddonVerifyFile = false;
let expectedClosingFenceMarker;
for await (const line of src.readLines()) {
  if (expectedClosingFenceMarker) {
    // We're inside a Addon snippet
    if (line === expectedClosingFenceMarker) {
      // End of the snippet
      expectedClosingFenceMarker = null;
      continue;
    }

    entries.at(-1).contentRef.content += `${line}\n`;
  }
  if (nextBlockIsAddonVerifyFile) {
    if (line) {
      expectedClosingFenceMarker = line.replace(/\w/g, '');
      nextBlockIsAddonVerifyFile = false;
    }
    continue;
  }
  const match = MARKER_RE.exec(line);
  if (match) {
    nextBlockIsAddonVerifyFile = true;
    const { filenames } = match.groups;
    // Use a single contentRef for files with identical contents
    const contentRef = { content: '' };
    for (const filename of filenames.split(/\s+/).filter(Boolean)) {
      const [dir, name] = filename.split('/');
      entries.push({ dir, name, contentRef });
    }
  }
}

// Collect files grouped by section directory name.
const sections = Map.groupBy(entries, (e) => e.dir);

let idx = 0;
for (const [name, files] of sections) {
  const dirName = `${String(++idx).padStart(2, '0')}_${name}`;
  const dir = join(values.output, dirName);
  mkdirSync(dir, { recursive: true });

  for (const file of files) {
    let { content } = file.contentRef;
    if (file.name === 'test.js') {
      content =
        "'use strict';\n" +
        "const common = require('../../common');\n" +
        content.replace(
          "'./build/Release/addon'",
          // eslint-disable-next-line no-template-curly-in-string
          '`./build/${common.buildType}/addon`',
        );
    }
    writeFileSync(join(dir, file.name), content);
  }

  // Generate binding.gyp
  const names = files.map((f) => f.name);
  writeFileSync(join(dir, 'binding.gyp'), JSON.stringify({
    targets: [{
      target_name: 'addon',
      sources: names,
      includes: ['../common.gypi'],
    }],
  }));

  console.log(`Generated ${dirName} with files: ${names.join(', ')}`);
}
