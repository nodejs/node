module.exports = preprocess;

var path = require('path');
var fs = require('fs');

var includeExpr = /^@include\s+([A-Za-z0-9-_]+)(?:\.)?([a-zA-Z]*)$/gmi;
var includeData = {};

function preprocess(inputFile, input, cb) {
  input = stripComments(input);
  processIncludes(inputFile, input, function (err, data) {
    if (err) return cb(err);

    cb(null, data);
  });
}

function stripComments(input) {
  return input.replace(/^@\/\/.*$/gmi, '');
}

function processIncludes(inputFile, input, cb) {
  var includes = input.match(includeExpr);
  if (includes === null) return cb(null, input);
  var errState = null;
  console.error(includes);
  var incCount = includes.length;
  if (incCount === 0) cb(null, input);
  includes.forEach(function(include) {
    var fname = include.replace(/^@include\s+/, '');
    if (!fname.match(/\.markdown$/)) fname += '.markdown';

    if (includeData.hasOwnProperty(fname)) {
      input = input.split(include).join(includeData[fname]);
      incCount--;
      if (incCount === 0) {
        return cb(null, input);
      }
    }

    var fullFname = path.resolve(path.dirname(inputFile), fname);
    fs.readFile(fullFname, 'utf8', function(er, inc) {
      if (errState) return;
      if (er) return cb(errState = er);
      preprocess(inputFile, inc, function(er, inc) {
        if (errState) return;
        if (er) return cb(errState = er);
        incCount--;
        includeData[fname] = inc;
        input = input.split(include+'\n').join(includeData[fname]+'\n');
        if (incCount === 0) {
          return cb(null, input);
        }
      });
    });
  });
}
