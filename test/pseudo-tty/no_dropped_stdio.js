// https://github.com/nodejs/node/issues/6456#issuecomment-219320599
// https://gist.github.com/isaacs/1495b91ec66b21d30b10572d72ad2cdd
'use strict';
require('../common');

// 1000 bytes wrapped at 50 columns
// \n turns into a double-byte character
// (48 + {2}) * 20 = 1000
var out = ('o'.repeat(48) + '\n').repeat(20);
// Add the remaining 24 bytes and terminate with an 'O'.
// This results in 1025 bytes, just enough to overflow the 1kb OS X TTY buffer.
out += 'o'.repeat(24) + 'O';

process.stdout.write(out);
process.exit(0);
