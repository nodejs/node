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
  assertTrue(out != "");

}).addErrback(function (code, out, err) {
  error_count++;

  assertEquals("", out);
  assertTrue(code != 0);

  puts("error!: " + code);
  puts("stdout: " + JSON.stringify(out));
  puts("stderr: " + JSON.stringify(err));
});


process.addListener("exit", function () {
  assertEquals(1, success_count);
  assertEquals(1, error_count);
});
