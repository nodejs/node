process.mixin(require("../common"));

var exit_status = -1;

var cat = process.createChildProcess("cat");

cat.addListener("output", function (chunk) { assert.equal(null, chunk); });
cat.addListener("error", function (chunk) { assert.equal(null, chunk); });
cat.addListener("exit", function (status) { exit_status = status; });

cat.kill();

process.addListener("exit", function () {
  assert.equal(true, exit_status > 0);
});
