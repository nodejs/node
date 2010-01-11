tcp = require("tcp");
http = require("http");

// This is a regression test for http://github.com/ry/node/issues/#issue/44
// It is separate from test-http-malformed-request.js because it is only
// reproduceable on the first packet on the first connection to a server.
port = 9999;

server = http.createServer(function (req, res) {});
server.listen(port);

tcp.createConnection(port).addListener("connect", function () {
        this.close();
}).addListener("close", function () {
	server.close();
});
