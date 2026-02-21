const fork = require('child_process').fork;
const path = require('path');

const child = fork(
	path.join(__dirname, 'child-process-persistent.js'),
	[],
	{ detached: true, stdio: 'ignore' }
);

console.log(child.pid);

child.unref();
