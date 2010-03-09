require("../common");

var n = parseInt(process.argv[2]);

var s = "";
for (var i = 0; i < n-1; i++) {
  s += 'c';
}

puts(s); // \n is the nth char.
