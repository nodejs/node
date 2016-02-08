'use strict'

const common = require('../common');
const domain = require('domain');
const assert = require('assert');
const fs = require('fs');
const d = domain.create()

const acc = [];

d.on('error', function(err) {
  acc.push(err.message);
});
d.run(function() {
  const p = fs.openAsync(__filename, 'r')
  
  p.then(function(fd) {
    fs.readFileAsync(fd, function() {
      throw new Error('one');
    });
    return fd;
  }, function() {
    setTimeout(function () {
      throw new Error('should not reach this point.')
    });
  }).then(function(fd) {
    setTimeout(function() {
      throw new Error('two')
    });
    return fd;
  }).then(function(fd) {
    return fs.closeAsync(fd);
  });
});

process.on('exit', function () {
  assert.ok(acc.indexOf('one') !== -1);
  assert.ok(acc.indexOf('two') !== -1);
  assert.equal(acc.length, 2);
})
