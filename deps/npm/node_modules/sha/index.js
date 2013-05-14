var crypto = require('crypto')
var fs
try {
  fs = require('graceful-fs')
} catch (ex) {
  fs = require('fs')
}
try {
  process.binding('crypto')
} catch (e) {
  var er = new Error( 'crypto binding not found.\n'
                    + 'Please build node with openssl.\n'
                    + e.message )
  throw er
}

exports.check = check
exports.get = get

function check(file, expected, options, cb) {
  if (typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  expected = expected.toLowerCase().trim();
  get(file, options, function (er, actual) {
    if (er) {
      if (er.message) er.message += ' while getting shasum for ' + file;
      return cb(er)
    }
    if (actual === expected) return cb(null);
    cb(new Error(
        'shasum check failed for ' + file + '\n'
      + 'Expected: ' + expected + '\n'
      + 'Actual:   ' + actual))
  })
}

function get(file, options, cb) {
  if (typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  options = options || {};
  var algorithm = options.algorithm || 'sha1';
  var hash = crypto.createHash(algorithm)
  var source = fs.createReadStream(file)
  var errState = null
  source
    .on('error', function (er) {
      if (errState) return
      return cb(errState = er)
    })
    .on('data', function (chunk) {
      if (errState) return
      hash.update(chunk)
    })
    .on('end', function () {
      if (errState) return
      var actual = hash.digest("hex").toLowerCase().trim()
      cb(null, actual)
    })
}
