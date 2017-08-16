var fs = require ('fs');
var net = require('net');
var join = require('path').join;
var file = join(__dirname, 'fixtures','all_npm.json');
var it = require('it-is');
var JSONStream = require('../');

var str = fs.readFileSync(file);

var server = net.createServer(function(client) {
    var data_calls = 0;
    var parser = JSONStream.parse();
    parser.on('error', function(err) {
        console.log(err);
        server.close();
    });

    parser.on('end', function() {
        console.log('END');
        server.close();
    });
    client.pipe(parser);
});
server.listen(9999);

var client = net.connect({ port : 9999 }, function() {
    var msgs = str + '}';
    client.end(msgs);
});
