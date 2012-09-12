var http = require('http');

$(function () {
    var opts = {
        host : window.location.hostname,
        port : window.location.port,
        path : '/count'
    };
    
    http.get(opts, function (res) {
        res.on('data', function (buf) {
            $('<div>')
                .text(buf)
                .appendTo($('#count'))
            ;
        });
        
        res.on('end', function () {
            $('<div>')
                .text('__END__')
                .appendTo($('#count'))
            ;
        });
    });
});
