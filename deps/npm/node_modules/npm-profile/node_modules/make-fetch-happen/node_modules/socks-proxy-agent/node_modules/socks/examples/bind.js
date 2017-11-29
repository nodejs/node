var Socks = require('../index.js');

var options = {
    proxy: {
        ipaddress: "202.101.228.108",
        port: 1080,
        type: 5,
        command: 'bind'
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

        // BIND request completed, now a tcp client should connect to this endpoint:
        console.log(info);
        // { port: 3334, host: '202.101.228.108' }

        // Resume! You need to!
        socket.resume();
    }
});