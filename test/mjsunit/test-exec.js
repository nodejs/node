process.mixin(require("./common"));

success_count = 0;
error_count = 0;

exec("ls /").addCallback(function (out) {
  success_count++;
  p(out);
}).addErrback(function (code, out, err) {
  error_count++;
  puts("error!: " + code);
  puts("stdout: " + JSON.stringify(out));
  puts("stderr: " + JSON.stringify(err));
});



exec("ls /DOES_NOT_EXIST").addCallback(function (out) {
  success_count++;
  p(out);
  assert.equal(true, out != "");

}).addErrback(function (code, out, err) {
  error_count++;

  assert.equal("", out);
  assert.equal(true, code != 0);

  puts("error!: " + code);
  puts("stdout: " + JSON.stringify(out));
  puts("stderr: " + JSON.stringify(err));
});


process.addListener("exit", function () {
  assert.equal(1, success_count);
  assert.equal(1, error_count);
});
