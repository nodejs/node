require("../common");
var path = require('path');

var sub = path.join(fixturesDir, 'print-chars.js');

n = 100000;

var child = process.createChildProcess(process.argv[0], [sub, n]);

var count = 0;

child.addListener("error", function (data){
  if (data) {
    puts("parent stderr: " + data);
    assert.ok(false);
  }
});

child.addListener("output", function (data){
  if (data) {
    count += data.length;
    puts(count);
  }
});

child.addListener("exit", function (data) {
  assert.equal(n, count);
  puts("okay");
});
