var should = require('should');
var sinon = require('sinon-restore');
var index = require('../index');
var fs = require('fs');
logger = require('log-driver')({level : false});

describe("handleInput", function(){
   afterEach(function() {
        sinon.restoreAll();
      });
  it ("returns an error when there's an error getting options", function(done){
    sinon.stub(index, 'getOptions', function(cb){
      return cb("some error", {}); 
    });
    var path = __dirname + "/../fixtures/onefile.lcov";
    var input = fs.readFileSync(path, "utf8");
    index.handleInput(input, function(err){
      err.should.equal("some error");
      done();
    });
  });
  it ("returns an error when there's an error converting", function(done){
    sinon.stub(index, 'getOptions', function(cb){
      return cb(null, {}); 
    });
    sinon.stub(index, 'convertLcovToCoveralls', function(input, options, cb){
      cb("some error");
    });
    var path = __dirname + "/../fixtures/onefile.lcov";
    var input = fs.readFileSync(path, "utf8");
    index.handleInput(input, function(err){
      err.should.equal("some error");
      done();
    });
  });
  it ("returns an error when there's an error sending", function(done){
    sinon.stub(index, 'getOptions', function(cb){
      return cb(null, {}); 
    });
    sinon.stub(index, 'sendToCoveralls', function(postData, cb){
      cb("some error");
    });
    var path = __dirname + "/../fixtures/onefile.lcov";
    var input = fs.readFileSync(path, "utf8");
    index.handleInput(input, function(err){
      err.should.equal("some error");
      done();
    });
  });
  it ("returns an error when there's a bad status code", function(done){
    sinon.stub(index, 'getOptions', function(cb){
      return cb(null, {}); 
    });
    sinon.stub(index, 'sendToCoveralls', function(postData, cb){
      cb(null, {statusCode : 500}, "body");
    });
    var path = __dirname + "/../fixtures/onefile.lcov";
    var input = fs.readFileSync(path, "utf8");
    index.handleInput(input, function(err){
      err.should.equal("Bad response: 500 body");
      done();
    });
  });
  it ("completes successfully when there are no errors", function(done){
    sinon.stub(index, 'getOptions', function(cb){
      return cb(null, {}); 
    });
    sinon.stub(index, 'sendToCoveralls', function(postData, cb){
      cb(null, {statusCode : 200}, "body");
    });
    var path = __dirname + "/../fixtures/onefile.lcov";
    var input = fs.readFileSync(path, "utf8");
    index.handleInput(input, function(err){
      (err === null).should.equal(true);
      done();
    });
  });
});
