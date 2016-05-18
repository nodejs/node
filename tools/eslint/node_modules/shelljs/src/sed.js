var common = require('./common');
var fs = require('fs');

//@
//@ ### sed([options,] search_regex, replacement, file [, file ...])
//@ ### sed([options,] search_regex, replacement, file_array)
//@ Available options:
//@
//@ + `-i`: Replace contents of 'file' in-place. _Note that no backups will be created!_
//@
//@ Examples:
//@
//@ ```javascript
//@ sed('-i', 'PROGRAM_VERSION', 'v0.1.3', 'source.js');
//@ sed(/.*DELETE_THIS_LINE.*\n/, '', 'source.js');
//@ ```
//@
//@ Reads an input string from `files` and performs a JavaScript `replace()` on the input
//@ using the given search regex and replacement string or function. Returns the new string after replacement.
function _sed(options, regex, replacement, files) {
  options = common.parseOptions(options, {
    'i': 'inplace'
  });

  if (typeof replacement === 'string' || typeof replacement === 'function')
    replacement = replacement; // no-op
  else if (typeof replacement === 'number')
    replacement = replacement.toString(); // fallback
  else
    common.error('invalid replacement string');

  // Convert all search strings to RegExp
  if (typeof regex === 'string')
    regex = RegExp(regex);

  if (!files)
    common.error('no files given');

  if (typeof files === 'string')
    files = [].slice.call(arguments, 3);
  // if it's array leave it as it is

  files = common.expand(files);

  var sed = [];
  files.forEach(function(file) {
    if (!fs.existsSync(file)) {
      common.error('no such file or directory: ' + file, true);
      return;
    }

    var result = fs.readFileSync(file, 'utf8').split('\n').map(function (line) {
      return line.replace(regex, replacement);
    }).join('\n');

    sed.push(result);

    if (options.inplace)
      fs.writeFileSync(file, result, 'utf8');
  });

  return common.ShellString(sed.join('\n'));
}
module.exports = _sed;
