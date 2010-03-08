require("../common");
child = process.createChildProcess('/usr/bin/env', [], {'HELLO' : 'WORLD'});
response = "";

child.addListener("output", function (chunk) {
  puts("stdout: " + JSON.stringify(chunk));
  if (chunk) response += chunk;
});

process.addListener('exit', function () {
 assert.ok(response.indexOf('HELLO=WORLD') >= 0);
});
