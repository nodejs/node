var Socks = require('../index.js');
var dgram = require('dgram');

var options = {
    proxy: {
        ipaddress: "202.101.228.108",
        port: 1080,
        type: 5,
        command: 'associate'
    },

    target: {
        host: "0.0.0.0",
        port: 0
    }
};

Socks.createConnection(options, function(err, socket, info) {
    if (err)
        console.log(err);
    else {
        console.log("Connected");

        // Associate request completed.
        // Now we can send properly formed UDP packet frames to this endpoint for forwarding:
        console.log(info);
        // { port: 4381, host: '202.101.228.108' }

        var udp = new dgram.Socket('udp4');
        var packet = SocksClient.createUDPFrame({ host: "1.2.3.4", port: 5454}, new Buffer("Hello"));
        udp.send(packet, 0, packet.length, info.port, info.host);
    }
});