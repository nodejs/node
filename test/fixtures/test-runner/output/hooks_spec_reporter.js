'use strict';
require('../../../common');
const fixtures = require('../../../common/fixtures');
const spawn = require('node:child_process').spawn;

const child = spawn(process.execPath,
                    ['--no-warnings', '--test-reporter', 'spec', fixtures.path('test-runner/output/hooks.js')],
                    { stdio: 'pipe' });

child.stdout.pipe(process.stdout);
child.stderr.pipe(process.stderr);
