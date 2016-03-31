var verror = require('../lib/verror');

var opname = 'read';
var err = new verror.VError('"%s" operation failed', opname);
console.log(err.message);
console.log(err.stack);
