var convertLcovToCoveralls = require('../index').convertLcovToCoveralls;
var getOptions = require('../index').getOptions;
var should = require('should');
var fs = require('fs');
var logger = require('../lib/logger');
var path = require('path');
logger = require('log-driver')({level : false});

describe("convertLcovToCoveralls", function(){
  it ("should convert a simple lcov file", function(done){
    process.env.TRAVIS_JOB_ID = -1;
    var lcovpath = __dirname + "/../fixtures/onefile.lcov";
    var input = fs.readFileSync(lcovpath, "utf8");
    var libpath = __dirname + "/../fixtures/lib";
    convertLcovToCoveralls(input, {filepath: libpath}, function(err, output){
      should.not.exist(err);
      output.source_files[0].name.should.equal("index.js");
      output.source_files[0].source.split("\n").length.should.equal(173);
      output.source_files[0].coverage[54].should.equal(0);
      output.source_files[0].coverage[60].should.equal(0);
      done();
    });
  });

  it ("should pass on all appropriate parameters from the environment", function(done){
    process.env.TRAVIS_JOB_ID = -1;
    process.env.COVERALLS_GIT_COMMIT = "GIT_HASH";
    process.env.COVERALLS_GIT_BRANCH = "master";
    process.env.COVERALLS_SERVICE_NAME = "SERVICE_NAME";
    process.env.COVERALLS_SERVICE_JOB_ID = "SERVICE_JOB_ID";
    process.env.COVERALLS_REPO_TOKEN = "REPO_TOKEN";
    process.env.CI_PULL_REQUEST = "https://github.com/fake/fake/pulls/123";
    process.env.COVERALLS_PARALLEL = "true";

    getOptions(function(err, options){
      var lcovpath = __dirname + "/../fixtures/onefile.lcov";
      var input = fs.readFileSync(lcovpath, "utf8");
      var libpath = "fixtures/lib";
      options.filepath = libpath;
      convertLcovToCoveralls(input, options, function(err, output){
        should.not.exist(err);
        output.service_pull_request.should.equal("123");
        output.parallel.should.equal(true);
        //output.git.should.equal("GIT_HASH");
        done();
      });
    });
  });
  it ("should work with a relative path as well", function(done){
    process.env.TRAVIS_JOB_ID = -1;
    var lcovpath = __dirname + "/../fixtures/onefile.lcov";
    var input = fs.readFileSync(lcovpath, "utf8");
    var libpath = "fixtures/lib";
    convertLcovToCoveralls(input, {filepath: libpath}, function(err, output){
      should.not.exist(err);
      output.source_files[0].name.should.equal("index.js");
      output.source_files[0].source.split("\n").length.should.equal(173);
      done();
    });
  });

  it ("should convert absolute input paths to relative", function(done){
    process.env.TRAVIS_JOB_ID = -1;
    var lcovpath = __dirname + "/../fixtures/istanbul.lcov";
    var input = fs.readFileSync(lcovpath, "utf8");
    var libpath = "/Users/deepsweet/Dropbox/projects/svgo/lib";
    var sourcepath = path.resolve(libpath, "svgo/config.js");

    var originalReadFileSync = fs.readFileSync;
    fs.readFileSync = function(filepath) {
      if (filepath === sourcepath) {
        return '';
      }

      return originalReadFileSync.apply(fs, arguments);
    };

    var originalExistsSync = fs.existsSync;
    fs.existsSync = function () { return true; };

    convertLcovToCoveralls(input, {filepath: libpath}, function(err, output){
      fs.readFileSync = originalReadFileSync;
      fs.existsSync = originalExistsSync;

      should.not.exist(err);
      output.source_files[0].name.should.equal(path.join("svgo", "config.js"));
      done();
    });
  });

  it ("should ignore files that do not exists", function(done){
    process.env.TRAVIS_JOB_ID = -1;
    var lcovpath = __dirname + "/../fixtures/istanbul.lcov";
    var input = fs.readFileSync(lcovpath, "utf8");
    var libpath = "/Users/deepsweet/Dropbox/projects/svgo/lib";
    var sourcepath = path.resolve(libpath, "svgo/config.js");

    var originalReadFileSync = fs.readFileSync;
    fs.readFileSync = function(filepath) {
      if (filepath === sourcepath) {
        return '';
      }

      return originalReadFileSync.apply(fs, arguments);
    };

    var originalExistsSync = fs.existsSync;
    fs.existsSync = function () { return false; };

    convertLcovToCoveralls(input, {filepath: libpath}, function(err, output){
      fs.readFileSync = originalReadFileSync;
      fs.existsSync = originalExistsSync;

      should.not.exist(err);
      output.source_files.should.be.empty();
      done();
    });
  });

});
