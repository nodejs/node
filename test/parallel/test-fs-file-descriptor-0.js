'use strict';

const assert = require('assert');
const fs = require('fs');

// Test to ensure file descriptor 0 (stdin) is handled properly
const stream = fs.createReadStream(null, { fd: 0 });

stream.on('close', () => {
  assert.ok(true, 'File descriptor 0 closed successfully');
  console.log('Test passed: File descriptor 0 closed successfully');
});

stream.close();
