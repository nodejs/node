'use strict';

const common = require('../common');

if (!common.hasIntl) {
  common.skip('missing Intl');
}

// Refs: https://github.com/nodejs/node/blob/1af63a90ca3a59ca05b3a12ad7dbea04008db7d9/configure.py#L1694-L1711
if (process.config.variables.icu_path !== 'deps/icu-small') {
  // If Node.js is configured to use its built-in ICU, it uses a strict subset
  // of ICU formed using `tools/icu/shrink-icu-src.py`, which is present in
  // `deps/icu-small`. It is not the same as configuring the build with
  // `./configure --with-intl=small-icu`. The latter only uses a subset of the
  // locales, i.e., it uses the English locale, `root,en`, by default and other
  // locales can also be specified using the `--with-icu-locales` option.
  common.skip('not using the icu data file present in deps/icu-small/source/data/in/icudt##l.dat.bz2');
}

const fixtures = require('../common/fixtures');

// This test ensures the correctness of the automated timezone upgrade PRs.

const { strictEqual } = require('assert');
const { readFileSync } = require('fs');

const expectedVersion = readFileSync(fixtures.path('tz-version.txt'), 'utf8').trim();
strictEqual(process.versions.tz, expectedVersion);
