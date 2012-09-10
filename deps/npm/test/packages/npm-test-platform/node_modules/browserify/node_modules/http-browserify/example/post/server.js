var express = require('express');
var app = express.createServer();
app.use(express.static(__dirname));

app.post('/plusone', function (req, res) {
    res.setHeader('content-type', 'text/plain');
    
    var s = '';
    req.on('data', function (buf) { s += buf.toString() });
    
    req.on('end', function () {
        var n = parseInt(s) + 1;
        res.end(n.toString());
    });
});

var browserify = require('browserify');
var bundle = browserify(__dirname + '/main.js');
app.use(bundle);

console.log('Listening on :8082');
app.listen(8082);
