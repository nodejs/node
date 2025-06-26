// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test the script building the DB.
 */

'use strict';

const assert = require('assert');
const { execSync } = require("child_process");
const fs = require('fs');
const path = require('path');
const tempy = require('tempy');

const helpers = require('./helpers.js');

function buildDb(inputDir, corpusName, outputDir) {
  execSync(
      `node build_db.js -i ${inputDir} -o ${outputDir} ${corpusName}`,
      {stdio: ['pipe']});
}

function validateDb(inputDir, outputIndex) {
  execSync(
      `node validate_db.js -i ${inputDir} -o ${outputIndex}`,
      {stdio: ['pipe']});
}

describe('DB tests', () => {
  // Test feeds an expression that does not apply.
  it('omits erroneous expressions', () => {
    const outPath = tempy.directory();
    buildDb('test_data/db', 'this', outPath);
    const indexFile = path.join(outPath, 'index.json');
    const indexJSON = JSON.parse(fs.readFileSync(indexFile), 'utf-8');
    assert.deepEqual(indexJSON, []);
  });

  // End to end test with various extracted and not extracted examples.
  it('build db e2e', () => {
    const outPath = tempy.directory();
    buildDb('test_data/db', 'e2e', outPath);
    helpers.assertExpectedPath('db/e2e_expected', outPath);
  });

  // End to end test creating a db with error-prone snippets. The validation
  // enhances the index by marking or pruning.
  it('validate db e2e', () => {
    const outPath = tempy.directory();
    buildDb('test_data/db', 'validate', outPath);
    helpers.assertExpectedPath('db/validate_expected', outPath);
    const validated = path.join(outPath, 'updated_index.json');
    validateDb(path.join(helpers.BASE_DIR, 'db/validate_expected'), validated);
    helpers.assertFile('db/validate_index_expected.json', validated);
  });
});
