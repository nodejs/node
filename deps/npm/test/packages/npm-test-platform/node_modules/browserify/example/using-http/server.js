#!/usr/bin/env node
var http = require('http');
var ecstatic = require('ecstatic')(__dirname);

var server = http.createServer(function (req, res) {
    if (req.url === '/count') {
        res.setHeader('content-type', 'multipart/octet-stream');
        
        var n = 10;
        var iv = setInterval(function () {
            res.write(n + '\r\n');
            
            if (--n === 0) {
                clearInterval(iv);
                res.end();
            }
        }, 250);
    }
    else ecstatic(req, res)
});

server.listen(8001);
console.log([
    'Listening on :8001',
    '',
    'To compile the build, do:',
    '    browserify entry.js -o bundle.js'
].join('\n'));
