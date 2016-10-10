var fs = require('fs'),
    errs = require('../lib/errs');
 
function safeReadStream(no_such_file, callback) {
  try { 
    return fs.createReadStream(no_such_file, callback);
  } catch (err) {
    return errs.handle(err, callback);
  }
}

// would throw, now even without a callback it gets picked as a stream
var file = fs.createReadStream('FileDoesNotExist.here');
file.on('error', function (err) { console.log(err); });