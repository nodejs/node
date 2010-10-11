var path = require("path");
var util = require("util");
var childProcess = require("child_process");
var benchmarks = [ "timers.js"
                 , "process_loop.js"
                 , "static_http_server.js"
                 ];

var benchmarkDir = path.dirname(__filename);

function exec (script, callback) {
  var start = new Date();
  var child = childProcess.spawn(process.argv[0], [path.join(benchmarkDir, script)]);
  child.addListener("exit", function (code) {
    var elapsed = new Date() - start;
    callback(elapsed, code);
  });
}

function runNext (i) {
  if (i >= benchmarks.length) return;
  util.print(benchmarks[i] + ": ");
  exec(benchmarks[i], function (elapsed, code) {
    if (code != 0) {
      console.log("ERROR  ");
    }
    console.log(elapsed);
    runNext(i+1);
  });
};
runNext(0);
