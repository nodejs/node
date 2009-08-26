include("mjsunit.js");

var pwd_called = false;

function pwd (callback) {
  var output = "";
  var process = node.createProcess("pwd");
  process.addListener("output", function (s) {
    puts("stdout: " + JSON.stringify(s));
    if (s) output += s;
  });
  process.addListener("exit", function (c) {
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

function onExit () {
  assertTrue(pwd_called);
}
