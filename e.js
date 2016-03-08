s = require("child_process").spawn

console.log('argv %j', process.argv);

if (process.argv[2]) {
	console.log('in child')
	var l = require("net").createServer().listen(7001)
	l.on('listening', function() {
		console.log(this.address());
	})
	return
}


console.log("parent",process.execPath)

// XXX stdio must be ignored or detach doesn't work - not mentioned in docs :-(
c=s(process.execPath, [__filename, 'child'], {detached: true, stdio: 'ignore'});

console.log('pid', c.pid)
c.unref()
