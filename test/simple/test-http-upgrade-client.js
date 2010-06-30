// Verify that the 'upgrade' header causes an 'upgrade' event to be emitted to
// the HTTP client. This test uses a raw TCP server to better control server
// behavior.

require('../common');

var http = require('http');
var net = require('net');
var sys = require('sys');

var PORT = 5000 + Math.floor(Math.random() * 1000);

// Parse a string of data, returning an object if headers are complete, and
// undefined otherwise
var parseHeaders = function(data) {
    var m = data.search(/\r\n\r\n/);
    if (!m) {
        return;
    }

    var o = {};
    data.substring(0, m.index).split('\r\n').forEach(function(h) {
        var foo = h.split(':');
        if (foo.length < 2) {
            return;
        }

        o[foo[0].trim().toLowerCase()] = foo[1].trim().toLowerCase();
    });

    return o;
};

// Create a TCP server
var srv = net.createServer(function(c) {
    var data = '';
    c.addListener('data', function(d) {
        data += d.toString('utf8');

        // We found the end of the headers; make sure that we have an 'upgrade'
        // header and send back a response
        var headers = parseHeaders(data);
        if (!headers) {
            return;
        }

        assert.ok('upgrade' in headers);

        c.write('HTTP/1.1 101\r\n');
        c.write('connection: upgrade\r\n');
        c.write('upgrade: ' + headers.upgrade + '\r\n');
        c.write('\r\n');
        c.write('nurtzo');

        c.end();
    });
});
srv.listen(PORT, '127.0.0.1');

var gotUpgrade = false;
var hc = http.createClient(PORT, '127.0.0.1');
hc.addListener('upgrade', function(req, socket, upgradeHead) {
    // XXX: This test isn't fantastic, as it assumes that the entire response
    //      from the server will arrive in a single data callback
    assert.equal(upgradeHead, 'nurtzo');

    socket.end();
    srv.close();

    gotUpgrade = true;
});
hc.request('/', {
    'Connection' : 'Upgrade',
    'Upgrade' : 'WebSocket'
}).end();

process.addListener('exit', function() {
    assert.ok(gotUpgrade);
});
