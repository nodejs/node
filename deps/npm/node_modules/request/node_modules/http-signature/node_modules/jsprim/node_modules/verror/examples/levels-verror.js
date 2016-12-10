var extsprintf = require('extsprintf');
var fs = require('fs');
var verror = require('../lib/verror');

function checkFile(filename, callback) {
	fs.stat(filename, function (err) {
		if (err)
			/* Annotate the "stat" error with what we were doing. */
			return (callback(new verror.VError(err,
			    'failed to check "%s"', filename)));

		/* ... */
		return (callback());
	});
}

function handleRequest(filename, callback) {
	checkFile('/nonexistent', function (err) {
		if (err)
			/* Annotate the "checkFile" error. */
			return (callback(new verror.VError(
			    err, 'request failed')));

		/* ... */
		return (callback());
	});
}

handleRequest('/nonexistent', function (err) {
	if (err) {
		console.log(err.message);
		console.log(extsprintf.sprintf('%r', err));
	}

    /* ... */
});
