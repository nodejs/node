var net = require('net');
var ip = require('ip');
var SmartBuffer = require('smart-buffer');

(function () {

    var COMMAND = {
        Connect: 0x01,
        Bind: 0x02,
        Associate: 0x03
    };

    var SOCKS4_RESPONSE = {
        Granted: 0x5A,
        Failed: 0x5B,
        Rejected: 0x5C,
        RejectedIdent: 0x5D
    };

    var SOCKS5_AUTH = {
        NoAuth: 0x00,
        GSSApi: 0x01,
        UserPass: 0x02
    };

    var SOCKS5_RESPONSE = {
        Granted: 0x00,
        Failure: 0x01,
        NotAllowed: 0x02,
        NetworkUnreachable: 0x03,
        HostUnreachable: 0x04,
        ConnectionRefused: 0x05,
        TTLExpired: 0x06,
        CommandNotSupported: 0x07,
        AddressNotSupported: 0x08
    };


    exports.createConnection = function (options, callback) {
        var socket = new net.Socket(), finished = false, buff = new SmartBuffer();

        // Defaults
        options.timeout = options.timeout || 10000;
        options.proxy.command = commandFromString(options.proxy.command);
        options.proxy.userid = options.proxy.userid || "";

        var auth = options.proxy.authentication || {};
        auth.username = auth.username || "";
        auth.password = auth.password || "";

        options.proxy.authentication = auth;

        // Connect & negotiation timeout
        function onTimeout() {
            finish(new Error("Connection Timed Out"), socket, null, callback);
        }
        socket.setTimeout(options.timeout, onTimeout);

        // Socket events
        socket.once('close', function () {
            finish(new Error("Socket Closed"), socket, null, callback);
        });

        socket.once('error', function (err) {
        });

        socket.once('connect', function () {
            if (options.proxy.type === 4) {
                negotiateSocks4(options, socket, callback);
            } else if (options.proxy.type === 5) {
                negotiateSocks5(options, socket, callback);
            } else {
                throw new Error("Please specify a proxy type in options.proxy.type");
            }
        });

        socket.connect(options.proxy.port, options.proxy.ipaddress);


        // 4/4a  (connect, bind) - Supports domains & ipaddress
        function negotiateSocks4(options, socket, callback) {
            buff.writeUInt8(0x04);
            buff.writeUInt8(options.proxy.command);
            buff.writeUInt16BE(options.target.port);

            // ipv4 or domain?
            if (net.isIPv4(options.target.host)) {
                buff.writeBuffer(ip.toBuffer(options.target.host));
                buff.writeStringNT(options.proxy.userid);
            } else {
                buff.writeUInt8(0x00);
                buff.writeUInt8(0x00);
                buff.writeUInt8(0x00);
                buff.writeUInt8(0x01);
                buff.writeStringNT(options.proxy.userid);
                buff.writeStringNT(options.target.host);
            }

            socket.once('data', receivedResponse);
            socket.write(buff.toBuffer());

            function receivedResponse(data) {
                socket.pause();
                if (data.length === 8 && data[1] === SOCKS4_RESPONSE.Granted) {

                    if (options.proxy.command === COMMAND.Bind) {
                        buff.clear();
                        buff.writeBuffer(data);
                        buff.skip(2);

                        var info = {
                            port: buff.readUInt16BE(),
                            host: buff.readUInt32BE()
                        };

                        if (info.host === 0) {
                            info.host = options.proxy.ipaddress;
                        } else {
                            info.host = ip.fromLong(info.host);
                        }

                        finish(null, socket, info, callback);
                    } else {
                        finish(null, socket, null, callback);
                    }

                } else {
                    finish(new Error("Rejected (" + data[1] + ")"), socket, null, callback);
                }
            }
        }

        // Socks 5 (connect, bind, associate) - Supports domains and ipv4, ipv6.
        function negotiateSocks5(options, socket, callback) {
            buff.writeUInt8(0x05);
            buff.writeUInt8(2);
            buff.writeUInt8(SOCKS5_AUTH.NoAuth);
            buff.writeUInt8(SOCKS5_AUTH.UserPass);

            socket.once('data', handshake);
            socket.write(buff.toBuffer());

            function handshake(data) {
                if (data.length !== 2) {
                    finish(new Error("Negotiation Error"), socket, null, callback);
                } else if (data[0] !== 0x05) {
                    finish(new Error("Negotiation Error (invalid version)"), socket, null, callback);
                } else if (data[1] === 0xFF) {
                    finish(new Error("Negotiation Error (unacceptable authentication)"), socket, null, callback);
                } else {
                    if (data[1] === SOCKS5_AUTH.NoAuth) {
                        sendRequest();
                    } else if (data[1] === SOCKS5_AUTH.UserPass) {
                        sendAuthentication(options.proxy.authentication);
                    } else {
                        finish(new Error("Negotiation Error (unknown authentication type)"), socket, null, callback);
                    }
                }
            }

            function sendAuthentication(authinfo) {
                buff.clear();
                buff.writeUInt8(0x01);
                buff.writeUInt8(Buffer.byteLength(authinfo.username));
                buff.writeString(authinfo.username);
                buff.writeUInt8(Buffer.byteLength(authinfo.password));
                buff.writeString(authinfo.password);

                socket.once('data', authenticationResponse);
                socket.write(buff.toBuffer());

                function authenticationResponse(data) {
                    if (data.length === 2 && data[1] === 0x00) {
                        sendRequest();
                    } else {
                        finish(new Error("Negotiation Error (authentication failed)"), socket, null, callback);
                    }
                }
            }

            function sendRequest() {
                buff.clear();
                buff.writeUInt8(0x05);
                buff.writeUInt8(options.proxy.command);
                buff.writeUInt8(0x00);

                // ipv4, ipv6, domain?
                if (net.isIPv4(options.target.host)) {
                    buff.writeUInt8(0x01);
                    buff.writeBuffer(ip.toBuffer(options.target.host));
                } else if (net.isIPv6(options.target.host)) {
                    buff.writeUInt8(0x04);
                    buff.writeBuffer(ip.toBuffer(options.target.host));
                } else {
                    buff.writeUInt8(0x03);
                    buff.writeUInt8(options.target.host.length);
                    buff.writeString(options.target.host);
                }
                buff.writeUInt16BE(options.target.port);

                socket.once('data', receivedResponse);
                socket.write(buff.toBuffer());
            }

            function receivedResponse(data) {
                socket.pause();
                if (data.length < 4) {
                    finish(new Error("Negotiation Error"), socket, null, callback);
                } else if (data[0] === 0x05 && data[1] === SOCKS5_RESPONSE.Granted) {
                    if (options.proxy.command === COMMAND.Connect) {
                        finish(null, socket, null, callback);
                    } else if (options.proxy.command === COMMAND.Bind || options.proxy.command === COMMAND.Associate) {
                        buff.clear();
                        buff.writeBuffer(data);
                        buff.skip(3);

                        var info = {};
                        var addrtype = buff.readUInt8();

                        try {

                            if (addrtype === 0x01) {
                                info.host = buff.readUInt32BE();
                                if (info.host === 0)
                                    info.host = options.proxy.ipaddress;
                                else
                                    info.host = ip.fromLong(info.host);
                            } else if (addrtype === 0x03) {
                                var len = buff.readUInt8();
                                info.host = buff.readString(len);
                            } else if (addrtype === 0x04) {
                                info.host = buff.readBuffer(16);
                            } else {
                                finish(new Error("Negotiation Error (invalid host address)"), socket, null, callback);
                            }
                            info.port = buff.readUInt16BE();

                            finish(null, socket, info, callback);
                        } catch (ex) {
                            finish(new Error("Negotiation Error (missing data)"), socket, null, callback);
                        }
                    }
                } else {
                    finish(new Error("Negotiation Error (" + data[1] + ")"), socket, null, callback);
                }
            }
        }

        function finish(err, socket, info, callback) {
            socket.setTimeout(0, onTimeout);
            if (!finished) {
                finished = true;

                if (buff instanceof SmartBuffer)
                    buff.destroy();

                if (err && socket instanceof net.Socket) {
                    socket.removeAllListeners('close');
                    socket.removeAllListeners('timeout');
                    socket.removeAllListeners('data');
                    socket.destroy();
                    socket = null;
                }

                callback(err, socket, info);
            }
        }

        function commandFromString(str) {
            var result = COMMAND.Connect;

            if (str === "connect") {
                result = COMMAND.Connect;
            } else if (str === 'associate') {
                result = COMMAND.Associate;
            } else if (str === 'bind') {
                result = COMMAND.Bind;
            }

            return result;
        }
    };


    exports.createUDPFrame = function (target, data, frame) {
        var buff = new SmartBuffer();
        buff.writeUInt16BE(0);
        buff.writeUInt8(frame || 0x00);

        if (net.isIPv4(target.host)) {
            buff.writeUInt8(0x01);
            buff.writeUInt32BE(ip.toLong(target.host));
        } else if (net.isIPv6(target.host)) {
            buff.writeUInt8(0x04);
            buff.writeBuffer(ip.toBuffer(target.host));
        } else {
            buff.writeUInt8(0x03);
            buff.writeUInt8(Buffer.byteLength(target.host));
            buff.writeString(target.host);
        }

        buff.writeUInt16BE(target.port);
        buff.writeBuffer(data);
        return buff.toBuffer();
    };
})();
