var assert = require('assert');

var n = parseInt(process.argv[2]);

var b = Buffer.allocUnsafe(n);
for (var i = 0; i < n; i++) {
  b[i] = 100;
}

process.stdout.write(b);
