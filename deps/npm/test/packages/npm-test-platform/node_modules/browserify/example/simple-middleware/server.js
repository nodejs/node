var connect = require('connect');
var server = connect.createServer();
server.use(connect.static(__dirname));

var browserify = require('browserify');
var bundle = browserify(__dirname + '/js/entry.js');
server.use(bundle);

var port = parseInt(process.argv[2] || 8080, 10);
server.listen(port);
console.log('Listening on :' + port);
