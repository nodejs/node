'use strict';
// This tests that pausing and resuming stdin does not hang and timeout
// when done in a child process.  See test/simple/test-stdin-pause-resume.js
const common = require('../common');
const child_process = require('child_process');
const path = require('path');
child_process.spawn(process.execPath,
                    [path.resolve(__dirname, 'test-stdin-pause-resume.js')]);
