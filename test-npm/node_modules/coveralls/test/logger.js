var should = require('should');
var sinon = require('sinon-restore');
var index = require('../index');

describe("logger", function(){
  it ("should log at debug level when --verbose is set", function(){
    index.options.verbose = true;
    var logger = require('../index').logger();
    logger.level.should.equal('warn');
  });

  it ("should log at debug level when NODE_COVERALLS_DEBUG is set in env", function(){
    index.options.verbose = false;
    process.env.NODE_COVERALLS_DEBUG = 1;
    var logger = require('../index').logger();
    logger.level.should.equal('warn');
  });

  it ("should log at debug level when NODE_COVERALLS_DEBUG is set in env as a string", function(){
    index.options.verbose = false;
    process.env.NODE_COVERALLS_DEBUG = '1';
    var logger = require('../index').logger();
    logger.level.should.equal('warn');
  });

  it ("should log at warn level when NODE_COVERALLS_DEBUG not set and no --verbose", function(){
    index.options.verbose = false;
    process.env.NODE_COVERALLS_DEBUG = 0;
    var logger = require('../index').logger();
    logger.level.should.equal('error');
  });
});
