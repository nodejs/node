'use strict';

// Tests that the minimum ICU version for Node.js is at least the minimum ICU
// version for V8.

require('../common');
const assert = require('assert');
const path = require('path');
const { readFileSync } = require('fs');

const srcRoot = path.join(__dirname, '..', '..');
const icuVersionsFile = path.join(srcRoot, 'tools', 'icu', 'icu_versions.json');
const { minimum_icu: minimumICU } = require(icuVersionsFile);
const v8SrcFile = path.join(srcRoot,
                            'deps', 'v8', 'src', 'objects', 'intl-objects.h');
const v8Src = readFileSync(v8SrcFile, { encoding: 'utf8' });
const v8MinimumICU = v8Src.match(/#define\s+V8_MINIMUM_ICU_VERSION\s+(\d+)/)[1];
assert.ok(minimumICU >= Number(v8MinimumICU),
          `minimum ICU version in ${icuVersionsFile} (${minimumICU}) ` +
          `must be at least that in ${v8SrcFile} (${Number(v8MinimumICU)})`);
