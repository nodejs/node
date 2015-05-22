'use strict';

const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const stream = require('stream');
const path = require('path');

const producer = fork(path.join(common.fixturesDir, 'stdout-producer.js'),
                      { silent: true });

var output = '';
const writable = new stream.Writable({
  write(chunk, _, next) {
    output += chunk.toString();
    next();
  }
});

producer.stdout.pipe(writable);
producer.stdout.on('close', function() {
  assert(output.endsWith('hey\n'),
      'Not all chunked writes were completed before process termination.');
});

producer.stderr.pipe(process.stderr);
