#!/usr/bin/env node

var connect = require('connect');
var server = connect.createServer();

server.use(connect.static(__dirname));
server.listen(8080);
console.log('Listening on :8080');
console.log('Make sure to run ./build.sh to generate browserify.js');
