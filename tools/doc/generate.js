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

'use strict';

const processIncludes = require('./preprocess.js');
const fs = require('fs');

// parse the args.
// Don't use nopt or whatever for this.  It's simple enough.

const args = process.argv.slice(2);
let format = 'json';
let template = null;
let inputFile = null;
let nodeVersion = null;
let analytics = null;

args.forEach(function(arg) {
  if (!arg.startsWith('--')) {
    inputFile = arg;
  } else if (arg.startsWith('--format=')) {
    format = arg.replace(/^--format=/, '');
  } else if (arg.startsWith('--template=')) {
    template = arg.replace(/^--template=/, '');
  } else if (arg.startsWith('--node-version=')) {
    nodeVersion = arg.replace(/^--node-version=/, '');
  } else if (arg.startsWith('--analytics=')) {
    analytics = arg.replace(/^--analytics=/, '');
  }
});

nodeVersion = nodeVersion || process.version;

if (!inputFile) {
  throw new Error('No input file specified');
}

fs.readFile(inputFile, 'utf8', function(er, input) {
  if (er) throw er;
  // process the input for @include lines
  processIncludes(inputFile, input, next);
});

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
      require('./html.js')(
        {
          input: input,
          filename: inputFile,
          template: template,
          nodeVersion: nodeVersion,
          analytics: analytics,
        },

        function(er, html) {
          if (er) throw er;
          console.log(html);
        }
      );
      break;

    default:
      throw new Error(`Invalid format: ${format}`);
  }
}
