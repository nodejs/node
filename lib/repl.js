// A repl library that you can include in your own code to get a runtime
// interface to your program. Just require("/repl.js").

node.stdio.open();

var buffered_cmd = '';
var trimmer = /^\s*(.+)\s*$/m;

exports.prompt = "node> ";

function displayPrompt () {
  print(buffered_cmd.length ? '...   ' : exports.prompt);
}

displayPrompt();

node.stdio.addListener("data", function (cmd) {
  var matches = trimmer.exec(cmd);

  if (matches && matches.length == 2) {
    cmd = matches[1];
    try {
      buffered_cmd += cmd;
      try {
        puts(JSON.stringify(eval(buffered_cmd)));
        buffered_cmd = '';
      } catch (e) { 
        if (!(e instanceof SyntaxError)) 
          throw e;
      }
    } catch (e) {
      puts('caught an exception: ' + e);
      buffered_cmd = '';
    }
  }
  displayPrompt();
});
