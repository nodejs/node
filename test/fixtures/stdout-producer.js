'use strict';

// Produce a very long string, so that stdout will have to break it into chunks.
var str = 'test\n';
for (var i = 0; i < 17; i++, str += str);

// Add something so that we can identify the end.
str += 'hey\n';

process.stdout.write(str);

// Close the process, attempting to close before
// all chunked stdout writes are done.
//
// This is required to achieve the regression described in
// https://github.com/nodejs/io.js/issues/784.
process.exit();
