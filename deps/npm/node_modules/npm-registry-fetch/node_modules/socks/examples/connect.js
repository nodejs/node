var Socks = require('../index.js');

var options = {
    proxy: {
        ipaddress: "31.193.133.9",
        port: 1081,
        type: 5  // (4 or 5)
    },

    target: {
        host: "173.194.33.103", // (google.com)
        port: 80
    }
};

Socks.createConnection(options, function (err, socket, info) {
    if (err)
        console.log(err);
    else {
        console.log("Connected");

        socket.on('data', function (data) {
            // do something with incoming data
        });

        // Please remember that sockets need to be resumed before any data will come in.
        socket.resume();

        // We can do whatever we want with the socket now.
    }
});