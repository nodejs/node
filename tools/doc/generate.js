'use strict';

const processIncludes = require('./preprocess.js');
const fs = require('fs');

// parse the args.
// Don't use nopt or whatever for this.  It's simple enough.

const args = process.argv.slice(2);
let format = 'json';
let template = null;
let filename = null;
let nodeVersion = null;
let analytics = null;

args.forEach(function(arg) {
  if (!arg.startsWith('--')) {
    filename = arg;
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

if (!filename) {
  throw new Error('No input file specified');
}

console.error('Input file = %s', filename);
fs.readFile(filename, 'utf8', (er, input) => {
  if (er) throw er;
  // process the input for @include lines
  processIncludes(filename, input, next);
});

function next(er, input) {
  if (er) throw er;
  switch (format) {
    case 'json':
      require('./json.js')(input, filename, (er, obj) => {
        console.log(JSON.stringify(obj, null, 2));
        if (er) throw er;
      });
      break;

    case 'html':
      require('./html')({ input, filename, template, nodeVersion, analytics },
                        (err, html) => {
                          if (err) throw err;
                          console.log(html);
                        });
      break;

    default:
      throw new Error(`Invalid format: ${format}`);
  }
}
