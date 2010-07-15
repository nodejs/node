common = require("../common");
assert = common.assert

var spawn = require('child_process').spawn;
child = spawn('/usr/bin/env', [], {'HELLO' : 'WORLD'});

response = "";

child.stdout.setEncoding('utf8');

child.stdout.addListener("data", function (chunk) {
  console.log("stdout: " + chunk);
  response += chunk;
});

process.addListener('exit', function () {
 assert.ok(response.indexOf('HELLO=WORLD') >= 0);
});
