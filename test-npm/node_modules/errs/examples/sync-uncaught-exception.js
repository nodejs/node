var fs = require('fs'),
    errs = require('../lib/errs');

process.on('uncaughtException', function(err) {
  console.log(errs.merge(err, {namespace: 'uncaughtException'}));
});

var file = fs.createReadStream('FileDoesNotExist.here');
