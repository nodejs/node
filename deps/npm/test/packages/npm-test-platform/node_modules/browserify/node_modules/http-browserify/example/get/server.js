var express = require('express');
var app = express.createServer();
app.use(express.static(__dirname));

app.get('/beep', function (req, res) {
    res.setHeader('content-type', 'text/plain');
    res.end('boop');
});

var browserify = require('browserify');
var bundle = browserify(__dirname + '/main.js');
app.use(bundle);

console.log('Listening on :8082');
app.listen(8082);
