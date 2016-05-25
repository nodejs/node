'use strict';

const processIncludes = require('./preprocess.js');
const fs = require('fs');
const path = require('path');
const toHTML = require('./html.js').toHTML;

// parse the args.
// Don't use nopt or whatever for this.  It's simple enough.

const args = process.argv.slice(2);
let format = 'json';
let template = null;
let inputFile = null;
let nodeVersion = null;

args.forEach(function(arg) {
  if (!arg.match(/^\-\-/)) {
    inputFile = arg;
  } else if (arg.match(/^\-\-format=/)) {
    format = arg.replace(/^\-\-format=/, '');
  } else if (arg.match(/^\-\-template=/)) {
    template = arg.replace(/^\-\-template=/, '');
  } else if (arg.match(/^\-\-node\-version=/)) {
    nodeVersion = arg.replace(/^\-\-node\-version=/, '');
  }
});

nodeVersion = nodeVersion || process.version;

if (!inputFile) {
  throw new Error('No input file specified');
}

console.error('Input file = %s', inputFile);
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
      toHTML(input, inputFile, template, nodeVersion, (er, html) => {
        if (er) throw er;
        const filename = `./out/doc/api/${path.basename(inputFile, 'md')}html`;
        fs.writeFile(filename, html, (err) => {
          if (err)
            console.log(err);
          console.log('generated:', filename);
        });
      });
      break;

    default:
      throw new Error('Invalid format: ' + format);
  }
}
