'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const { internalModuleReadFile } = process.binding('fs');
const { readFileSync } = require('fs');
const { strictEqual } = require('assert');

strictEqual(internalModuleReadFile('nosuchfile'), undefined);
strictEqual(internalModuleReadFile(fixtures.path('empty.txt')), '');
strictEqual(internalModuleReadFile(fixtures.path('empty-with-bom.txt')), '');
{
  const filename = fixtures.path('require-bin/package.json');
  strictEqual(internalModuleReadFile(filename), readFileSync(filename, 'utf8'));
}
