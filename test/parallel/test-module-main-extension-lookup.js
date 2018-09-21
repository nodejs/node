'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const { execFileSync } = require('child_process');

const node = process.argv[0];

execFileSync(node, ['--experimental-modules',
                    fixtures.path('es-modules', 'test-esm-ok')]);
execFileSync(node, ['--experimental-modules',
                    fixtures.path('es-modules', 'noext')]);
