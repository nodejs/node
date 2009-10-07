node.mixin(require("common.js"));

if (process.ARGV[2] === "-child") {
  node.stdio.open();
  var handler = new node.SignalHandler(node.SIGUSR1);
  handler.addListener("signal", function() {
    node.stdio.write("handled SIGUSR1");
    setTimeout(function () {
      // Allow some time for the write to go through the pipez
      process.exit(0);
    }, 50);
  });
  debug("CHILD!!!");
  
} else {

  var child = node.createChildProcess(ARGV[0], ['--', ARGV[1], '-child']);

  var output = "";
  
  child.addListener('output', function (chunk) {
    puts("Child (stdout) said: " + JSON.stringify(chunk));
    if (chunk) { output += chunk };
  });

  child.addListener('error', function (chunk) {
    if (/CHILD!!!/.exec(chunk)) {
      puts("Sending SIGUSR1 to " + child.pid);
      child.kill(node.SIGUSR1)
    }
    puts("Child (stderr) said: " + JSON.stringify(chunk));
  });

  process.addListener("exit", function () {
    assertEquals("handled SIGUSR1", output);
  });
  
}
