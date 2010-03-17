require("../common");

var n = parseInt(process.argv[2]);

var s = "";
for (var i = 0; i < n; i++) {
  s += 'c';
}

process.stdout.write(s);
