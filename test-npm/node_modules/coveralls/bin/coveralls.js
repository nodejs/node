#!/usr/bin/env node
var handleInput = require('../lib/handleInput');
var logger = require('../lib/logger');


process.stdin.resume();
process.stdin.setEncoding('utf8');

var input = '';

process.stdin.on('data', function(chunk) {
    input += chunk;
});

process.stdin.on('end', function() {
    handleInput(input, function(err) {
      if (err) {
        throw err;
      }
    });
});

