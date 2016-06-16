'use strict';

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const path = require('path');
const readline = require('readline');
const fs = require('fs');

var validationCount = 0;
var errorScript = path.join(common.fixturesDir, 'throws_error.js');

// Run child process, expected to throw an error and generate a NodeReport
console.log('test-nodereport.js: running child process to create NodeReport');
var child = spawn(process.execPath,
                  ['--nodereport-events=exception', errorScript],
                  {cwd: common.tmpDir});

child.on('exit', function(code, signal) {
  assert.ok(code !== 0);
  assert.strictEqual(signal, null);

  // Locate and validate the NodeReport
  fs.readdir(common.tmpDir, function(err, files) {
    for (var i = 0; i < files.length; i++) {
      if (files[i].substring(0, 10) == 'NodeReport' &&
          files[i].indexOf(child.pid) != -1) {
        // Located NodeReport with expected PID
        console.log('test-nodereport.js: located NodeReport: ',
                    path.join(common.tmpDir, files[i]));
        const reader = readline.createInterface({
          input: fs.createReadStream(path.join(common.tmpDir, files[i]))
        });
        reader.on('line', (line) => {
          //console.log('Line from file:', line);
          if ((line.indexOf('==== NodeReport') > -1) ||
              (line.indexOf('==== Javascript stack trace') > -1) ||
              (line.indexOf('==== Javascript Heap') > -1) ||
              (line.indexOf('==== System Information') > -1)) {
            validationCount++; // found expected section in NodeReport
          }
        });
        fs.unlink(path.join(common.tmpDir, files[i]));
      }
    }
  });
});

process.on('exit', function() {
  console.log('test-nodereport.js: validation count: ',
              validationCount, ' (expected: 4)');
  assert.equal(4, validationCount);
});
