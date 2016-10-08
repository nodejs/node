var TRAVIS_JOB_ID = process.env.TRAVIS_JOB_ID || 'unknown';
var fs = require('fs');
var lcovParse = require('lcov-parse');
var path = require('path');
var logger = require('./logger')();

var detailsToCoverage = function(length, details){
  var coverage = new Array(length);
  details.forEach(function(obj){
    coverage[obj.line - 1] = obj.hit;
  });
  return coverage;
};

var convertLcovFileObject = function(file, filepath){
  var rootpath = filepath;
  filepath = path.resolve(rootpath, file.file);
	var source = fs.readFileSync(filepath, 'utf8');
	var lines = source.split("\n");
	var coverage = detailsToCoverage(lines.length, file.lines.details);
	return { name     : path.relative(rootpath, path.resolve(rootpath, file.file)).split( path.sep ).join( "/" ),
           source   : source,
           coverage : coverage	};
};

var convertLcovToCoveralls = function(input, options, cb){
  var filepath = options.filepath || '';
  logger.debug("in: ", filepath);
  filepath = path.resolve(process.cwd(), filepath);
  lcovParse(input, function(err, parsed){
    if (err){
      logger.error("error from lcovParse: ", err);
      logger.error("input: ", input);
      return cb(err);
    }
    var postJson = {
      source_files : []
    };
    if (options.git){
      postJson.git = options.git;
    }
    if (options.run_at){
      postJson.run_at = options.run_at;
    }
    if (options.service_name){
      postJson.service_name = options.service_name;
    }
    if (options.service_job_id){
      postJson.service_job_id = options.service_job_id;
    }
    if (options.service_pull_request) {
      postJson.service_pull_request = options.service_pull_request;
    }
    if (options.repo_token) {
      postJson.repo_token = options.repo_token;
    }
    if (options.parallel) {
      postJson.parallel = options.parallel;
    }
    if (options.service_pull_request) {
      postJson.service_pull_request = options.service_pull_request;
    }
    parsed.forEach(function(file){
      var currentFilePath = path.resolve(filepath, file.file);
      if (fs.existsSync(currentFilePath)) {
        postJson.source_files.push(convertLcovFileObject(file, filepath));
      }
    });
    return cb(null, postJson);
  });
};

module.exports = convertLcovToCoveralls;

/* example coveralls json file


{
  "service_job_id": "1234567890",
  "service_name": "travis-ci",
  "source_files": [
    {
      "name": "example.rb",
      "source": "def four\n  4\nend",
      "coverage": [null, 1, null]
    },
    {
      "name": "two.rb",
      "source": "def seven\n  eight\n  nine\nend",
      "coverage": [null, 1, 0, null]
    }
  ]
}


example output from lcov parser:

 [
  {
    "file": "index.js",
    "lines": {
      "found": 0,
      "hit": 0,
      "details": [
        {
          "line": 1,
          "hit": 1
        },
        {
          "line": 2,
          "hit": 1
        },
        {
          "line": 3,
          "hit": 1
        },
        {
          "line": 5,
          "hit": 1
        },

*/
