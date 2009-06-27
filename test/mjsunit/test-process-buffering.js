include("mjsunit.js");

var pwd_called = false;

function pwd (callback) {
  var output = "";
  var process = new node.Process("pwd");
  process.addListener("Output", function (s) {
    if (s) output += s;
  });
  process.addListener("Exit", function(c) {
    assertEquals(0, c);
    callback(output);
    pwd_called = true;
  });
}


function onLoad () {
  pwd(function (result) {
    print(result);  
    assertTrue(result.length > 1);
    assertEquals("\n", result[result.length-1]);
  });
}

function onExit () {
  assertTrue(pwd_called);
}
