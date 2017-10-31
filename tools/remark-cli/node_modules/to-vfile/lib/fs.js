'use strict';

var fs = require('fs');
var vfile = require('./core');

module.exports = vfile;

vfile.read = read;
vfile.readSync = readSync;
vfile.write = write;
vfile.writeSync = writeSync;

/* Create a virtual file and read it in, asynchronously. */
function read(description, options, callback) {
  var file = vfile(description);

  if (!callback) {
    callback = options;
    options = null;
  }

  fs.readFile(file.path, options, function (err, res) {
    if (err) {
      callback(err);
    } else {
      file.contents = res;
      callback(null, file);
    }
  });
}

/* Create a virtual file and read it in, synchronously. */
function readSync(description, options) {
  var file = vfile(description);
  file.contents = fs.readFileSync(file.path, options);
  return file;
}

/* Create a virtual file and write it out, asynchronously. */
function write(description, options, callback) {
  var file = vfile(description);

  /* Weird, right? Otherwise `fs` doesn’t accept it. */
  if (!callback) {
    callback = options;
    options = undefined;
  }

  fs.writeFile(file.path, file.contents || '', options, callback);
}

/* Create a virtual file and write it out, synchronously. */
function writeSync(description, options) {
  var file = vfile(description);
  fs.writeFileSync(file.path, file.contents || '', options);
}
