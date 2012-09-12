var http = require('http-browserify');

var opts = { path : '/beep', method : 'GET' };
var req = http.request(opts, function (res) {
    var div = document.getElementById('result');
    
    for (var key in res.headers) {
        div.innerHTML += key + ': ' + res.getHeader(key) + '<br>';
    }
    div.innerHTML += '<br>';
    
    res.on('data', function (buf) {
        div.innerHTML += buf;
    });
});

req.setHeader('bling', 'blong');
req.end();
