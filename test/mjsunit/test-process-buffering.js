process.mixin(require("./common"));

var pwd_called = false;

function pwd (callback) {
  var output = "";
  var child = process.createChildProcess("pwd");
  child.addListener("output", function (s) {
    puts("stdout: " + JSON.stringify(s));
    if (s) output += s;
  });
  child.addListener("exit", function (c) {
    puts("exit: " + c);
    assertEquals(0, c);
    callback(output);
    pwd_called = true;
  });
}


pwd(function (result) {
  p(result);
  assertTrue(result.length > 1);
  assertEquals("\n", result[result.length-1]);
});

process.addListener("exit", function () {
  assertTrue(pwd_called);
});
