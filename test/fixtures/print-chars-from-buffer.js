require("../common");
Buffer = require("buffer").Buffer;

var n = parseInt(process.argv[2]);

b = new Buffer(n);
for (var i = 0; i < n; i++) { b[i] = 100; }

process.stdout.write(b);
