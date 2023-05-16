'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { pathToFileURL } = require('url');

{
  assert.rejects(import('./'), /ERR_UNSUPPORTED_DIR_IMPORT/);
  assert.rejects(
    import(pathToFileURL(fixtures.path('packages', 'main'))),
    /Did you mean/,
  );
}
