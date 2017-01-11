const assert = require('assert');

const n = parseInt(process.argv[2]);

const b = Buffer.allocUnsafe(n);
for (let i = 0; i < n; i++) {
  b[i] = 100;
}

process.stdout.write(b);
