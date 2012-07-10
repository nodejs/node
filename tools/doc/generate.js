#!/usr/bin/env node
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var marked = require('marked');
var fs = require('fs');
var path = require('path');

// parse the args.
// Don't use nopt or whatever for this.  It's simple enough.

var args = process.argv.slice(2);
var format = 'json';
var template = null;
var inputFile = null;

args.forEach(function (arg) {
  if (!arg.match(/^\-\-/)) {
    inputFile = arg;
  } else if (arg.match(/^\-\-format=/)) {
    format = arg.replace(/^\-\-format=/, '');
  } else if (arg.match(/^\-\-template=/)) {
    template = arg.replace(/^\-\-template=/, '');
  }
})


if (!inputFile) {
  throw new Error('No input file specified');
}


console.error('Input file = %s', inputFile);
fs.readFile(inputFile, 'utf8', function(er, input) {
  if (er) throw er;
  // process the input for @include lines
  processIncludes(input, next);
});


var includeExpr = /^@include\s+([A-Za-z0-9-_]+)(?:\.)?([a-zA-Z]*)$/gmi;
var includeData = {};
function processIncludes(input, cb) {
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
      processIncludes(inc, function(er, inc) {
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


function next(er, input) {
  if (er) throw er;
  switch (format) {
    case 'json':
      require('./json.js')(input, inputFile, function(er, obj) {
        console.log(JSON.stringify(obj, null, 2));
        if (er) throw er;
      });
      break;

    case 'html':
      require('./html.js')(input, inputFile, template, function(er, html) {
        if (er) throw er;
        console.log(html);
      });
      break;

    default:
      throw new Error('Invalid format: ' + format);
  }
}
