var express = require('express');
var app = express.createServer();
app.use(express.static(__dirname));

app.get('/doom', function (req, res) {
    res.setHeader('content-type', 'multipart/octet-stream');
    
    res.write('d');
    var i = 0;
    var iv = setInterval(function () {
        res.write('o');
        if (i++ >= 10) {
            clearInterval(iv);
            res.end('m');
        }
    }, 500);
});

var browserify = require('browserify');
var bundle = browserify(__dirname + '/main.js');
app.use(bundle);

console.log('Listening on :8082');
app.listen(8082);
