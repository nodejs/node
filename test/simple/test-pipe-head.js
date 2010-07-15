common = require("../common");
assert = common.assert

exec = require('child_process').exec;
join = require('path').join;

nodePath = process.argv[0];
script = join(common.fixturesDir, 'print-10-lines.js');

cmd = nodePath + ' ' + script + ' | head -2';

finished = false;

exec(cmd, function (err, stdout, stderr) {
  if (err) throw err;
  lines = stdout.split('\n');
  assert.equal(3, lines.length);
  finished = true;
});


process.addListener('exit', function () {
  assert.ok(finished);
});
