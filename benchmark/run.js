var path = require("path");
libDir = path.join(path.dirname(__filename), "../lib");
require.paths.unshift(libDir);
process.mixin(require("sys"));
var benchmarks = [ "static_http_server.js" 
                 , "timers.js"
                 , "process_loop.js"
                 ];

var benchmark_dir = path.dirname(__filename);

function exec (script, callback) {
  var start = new Date();
  var child = process.createChildProcess(process.ARGV[0], [path.join(benchmark_dir, script)]);
  child.addListener("exit", function (code) {
    var elapsed = new Date() - start;
    callback(elapsed, code);
  });
}

function runNext (i) {
  if (i >= benchmarks.length) return;
  print(benchmarks[i] + ": ");
  exec(benchmarks[i], function (elapsed, code) {
    if (code != 0) {
      puts("ERROR  ");
    }
    puts(elapsed);
    runNext(i+1);
  });
};
runNext(0);
