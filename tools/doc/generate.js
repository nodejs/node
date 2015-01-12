// USE OR OTHER DEALINGS IN THE SOFTWARE.

var processIncludes = require('./preprocess.js');
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
      require('./html.js')(input, inputFile, template, function(er, html) {
        if (er) throw er;
        console.log(html);
      });
      break;

    default:
      throw new Error('Invalid format: ' + format);
  }
}
