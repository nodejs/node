var mod_fs = require('fs');
var mod_verror = require('../lib/verror');

var filename = '/nonexistent';

mod_fs.stat(filename, function (err1) {
	var err2 = new mod_verror.VError(err1, 'failed to stat "%s"', filename);

	/* The following would normally be higher up the stack. */
	var err3 = new mod_verror.VError(err2, 'failed to handle request');
	console.log(err3.message);
	console.log(err3.stack);
});
