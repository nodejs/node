require("../common");

var N = 40;
var finished = false;

function spawn (i) {
  var child = process.createChildProcess( 'python'
                                     , ['-c', 'print 500 * 1024 * "C"']
                                     );
  var output = "";

  child.addListener("output", function(chunk) {
    if (chunk) output += chunk;
  });

  child.addListener("error", function(chunk) {
    if (chunk) error(chunk)
  });

  child.addListener("exit", function () {
    puts(output);
    if (i < N)
      spawn(i+1);
    else
      finished = true;
  });
}

spawn(0);

process.addListener("exit", function () {
  assert.equal(true, finished);
});
