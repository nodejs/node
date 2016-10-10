'use strict';

var getCallerFile = require('./');
var expect = require('chai').expect;
var foo = require('./fixtures/foo');
var bar = require('./fixtures/bar');
var ensurePosix = require('ensure-posix-path');

describe('getCallerFile', function() {
  it('gets current caller file', function() {
    expect(ensurePosix(getCallerFile())).to.eql(ensurePosix(__dirname + '/node_modules/mocha/lib/runnable.js'));
  });

  it('gets current file, as it is the caller', function() {
    expect(ensurePosix(foo())).to.eql(ensurePosix(__dirname + '/test.js'));
  });

  it('gets another file, as it is the caller', function() {
    expect(ensurePosix(bar())).to.eql(ensurePosix(__dirname + '/fixtures/bar.js'));
  });
});
