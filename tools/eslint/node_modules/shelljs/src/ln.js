var fs = require('fs');
var path = require('path');
var common = require('./common');

//@
//@ ### ln([options,] source, dest)
//@ Available options:
//@
//@ + `-s`: symlink
//@ + `-f`: force
//@
//@ Examples:
//@
//@ ```javascript
//@ ln('file', 'newlink');
//@ ln('-sf', 'file', 'existing');
//@ ```
//@
//@ Links source to dest. Use -f to force the link, should dest already exist.
function _ln(options, source, dest) {
  options = common.parseOptions(options, {
    's': 'symlink',
    'f': 'force'
  });

  if (!source || !dest) {
    common.error('Missing <source> and/or <dest>');
  }

  source = String(source);
  var sourcePath = path.normalize(source).replace(RegExp(path.sep + '$'), '');
  var isAbsolute = (path.resolve(source) === sourcePath);
  dest = path.resolve(process.cwd(), String(dest));

  if (fs.existsSync(dest)) {
    if (!options.force) {
      common.error('Destination file exists', true);
    }

    fs.unlinkSync(dest);
  }

  if (options.symlink) {
    var isWindows = common.platform === 'win';
    var linkType = isWindows ? 'file' : null;
    var resolvedSourcePath = isAbsolute ? sourcePath : path.resolve(process.cwd(), path.dirname(dest), source);
    if (!fs.existsSync(resolvedSourcePath)) {
      common.error('Source file does not exist', true);
    } else if (isWindows && fs.statSync(resolvedSourcePath).isDirectory()) {
      linkType =  'junction';
    }

    try {
      fs.symlinkSync(linkType === 'junction' ? resolvedSourcePath: source, dest, linkType);
    } catch (err) {
      common.error(err.message);
    }
  } else {
    if (!fs.existsSync(source)) {
      common.error('Source file does not exist', true);
    }
    try {
      fs.linkSync(source, dest);
    } catch (err) {
      common.error(err.message);
    }
  }
}
module.exports = _ln;
