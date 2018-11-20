'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const { internalModuleReadJSON } = process.binding('fs');
const { readFileSync } = require('fs');
const { strictEqual } = require('assert');

strictEqual(internalModuleReadJSON('nosuchfile'), undefined);
strictEqual(internalModuleReadJSON(fixtures.path('empty.txt')), undefined);
strictEqual(internalModuleReadJSON(fixtures.path('empty-with-bom.txt')),
            undefined);
{
  const filename = fixtures.path('require-bin/package.json');
  strictEqual(internalModuleReadJSON(filename), readFileSync(filename, 'utf8'));
}
