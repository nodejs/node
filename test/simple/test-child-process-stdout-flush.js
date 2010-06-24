require("../common");
var path = require('path');
var spawn = require('child_process').spawn;
var sub = path.join(fixturesDir, 'print-chars.js');

n = 500000;

var child = spawn(process.argv[0], [sub, n]);

var count = 0;

child.stderr.setEncoding('utf8');
child.stderr.addListener("data", function (data) {
  console.log("parent stderr: " + data);
  assert.ok(false);
});

child.stderr.setEncoding('utf8');
child.stdout.addListener("data", function (data) {
  count += data.length;
  console.log(count);
});

child.addListener("exit", function (data) {
  assert.equal(n, count);
  console.log("okay");
});
