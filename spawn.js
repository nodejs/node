var fs = require('fs');
var spawn = require("child_process").spawn;

console.log('argv %j', process.argv);

if (process.argv[2]) {
	console.log('in child')
	var l = require("http").createServer(fn).listen(7001)
	l.on('listening', function() {
		console.log(this.address());
	})
        function fn(req, rsp) {
          rsp.end('HELLO\n');
        }
	return
}


console.log("parent",process.execPath)

options = {
  log: 0, //true,
  logFile: 'error.log',
};

// Ignore stdin, it causes the detached process to still be associated
// with the terminal (on Windows).
var stdio = ['ignore', process.stdout, process.stderr];

if (options.log) {
  // Create or truncate file (append-mode has no append-after-truncate).
  fs.openSync(options.logFile, 'w');

  // Early versions of node would close an fd that was used multiple times in
  // a spawn `stdio` options.property, so open independent fds, in append
  // mode so they won't conflict.
  stdio[1] = fs.openSync(options.logFile, 'a');
  stdio[2] = fs.openSync(options.logFile, 'a');

  console.log('child logs to %j', options.logFile);
}

  var child = spawn(process.execPath, [__filename, 'child'], {
    detached: true,
    stdio: stdio,
  });

  child.on('error', function(err) {
    console.error(err.message);
  });

  child.unref();
