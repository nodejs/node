include("mjsunit.js");

var N = 40;
var finished = false;

function spawn (i) {
  var p = node.createProcess('python -c "print 500 * 1024 * \'C\'"');
  var output = "";

  p.addListener("output", function(chunk) {
    if (chunk) output += chunk;
  });

  p.addListener("exit", function () {
    //puts(output);
    if (i < N)
      spawn(i+1);
    else
      finished = true;
  });
}

spawn(0);

process.addListener("exit", function () {
  assertTrue(finished);
});
