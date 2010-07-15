common = require("../common");
assert = common.assert
net = require("net");
http = require("http");

// This is a regression test for http://github.com/ry/node/issues/#issue/44
// It is separate from test-http-malformed-request.js because it is only
// reproduceable on the first packet on the first connection to a server.

server = http.createServer(function (req, res) {});
server.listen(common.PORT);

net.createConnection(common.PORT).addListener("connect", function () {
  this.close();
}).addListener("close", function () {
	server.close();
});
