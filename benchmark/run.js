var path = require("path");
var sys = require("../lib/sys");
var benchmarks = [ "static_http_server.js" 
                 , "timers.js"
                 , "process_loop.js"
                 ];

var benchmarkDir = path.dirname(__filename);

function exec (script, callback) {
  var start = new Date();
  var child = process.createChildProcess(process.ARGV[0], [path.join(benchmarkDir, script)]);
  child.addListener("exit", function (code) {
    var elapsed = new Date() - start;
    callback(elapsed, code);
  });
}

function runNext (i) {
  if (i >= benchmarks.length) return;
  sys.print(benchmarks[i] + ": ");
  exec(benchmarks[i], function (elapsed, code) {
    if (code != 0) {
      sys.puts("ERROR  ");
    }
    sys.puts(elapsed);
    runNext(i+1);
  });
};
runNext(0);
