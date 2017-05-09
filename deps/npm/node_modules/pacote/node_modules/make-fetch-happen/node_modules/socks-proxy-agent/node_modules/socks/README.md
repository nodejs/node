socks
=============

socks is a full client implementation of the SOCKS 4, 4a, and 5 protocols in an easy to use node.js module.

### Notice
As of February 26th, 2015, socks is the new home of the socks-client package.

### Why socks?

There is not any other SOCKS proxy client library on npm that supports all three variants of the SOCKS protocol. Nor are there any that support the BIND and associate features that some versions of the SOCKS protocol supports.

Key Features:
* Supports SOCKS 4, 4a, and 5 protocols
* Supports the connect method (simple tcp connections of SOCKS)  (Client -> SOCKS Server -> Target Server)
* Supports the BIND method (4, 4a, 5)
* Supports the associate (UDP forwarding) method (5)
* Simple and easy to use (one function call to make any type of SOCKS connection)

## Installing:

`npm install socks`

### Getting Started Example

For this example, say you wanted to grab the html of google's home page.

```javascript
var Socks = require('socks');

var options = {
    proxy: {
        ipaddress: "202.101.228.108", // Random public proxy
        port: 1080,
        type: 5 // type is REQUIRED. Valid types: [4, 5]  (note 4 also works for 4a)
    },
    target: {
        host: "google.com", // can be an ip address or domain (4a and 5 only)
        port: 80
    },
    command: 'connect'  // This defaults to connect, so it's optional if you're not using BIND or Associate.
};

Socks.createConnection(options, function(err, socket, info) {
    if (err)
        console.log(err);
    else {
        // Connection has been established, we can start sending data now:
        socket.write("GET / HTTP/1.1\nHost: google.com\n\n");
        socket.on('data', function(data) {
            console.log(data.length);
            console.log(data);
        });

        // PLEASE NOTE: sockets need to be resumed before any data will come in or out as they are paused right before this callback is fired.
        socket.resume();

        // 569
        // <Buffer 48 54 54 50 2f 31 2e 31 20 33 30 31 20 4d 6f 76 65 64 20 50 65...
    }
});
```

### BIND Example:

When sending the BIND command to a SOCKS proxy server, this will cause the proxy server to open up a new tcp port. Once this port is open, you, another client, application, etc, can then connect to the SOCKS proxy on that tcp port and communications will be forwarded to each connection through the proxy itself.

```javascript
var options = {
    proxy: {
        ipaddress: "202.101.228.108",
        port: 1080,
        type: 4,
        command: "bind" // Since we are using bind, we must specify it here.
    },
    target: {
        host: "1.2.3.4", // When using bind, it's best to give an estimation of the ip that will be connecting to the newly opened tcp port on the proxy server.
        port: 1080
    }
};

Socks.createConnection(options, function(err, socket, info) {
    if (err)
        console.log(err);
    else {
        // BIND request has completed.
        // info object contains the remote ip and newly opened tcp port to connect to.
        console.log(info);

        // { port: 1494, host: '202.101.228.108' }

        socket.on('data', function(data) {
            console.log(data.length);
            console.log(data);
        });

        // Remember to resume the socket stream.
        socket.resume();
    }
});

```
At this point, your original connection to the proxy server remains open, and no data will be received until a tcp connection is made to the given endpoint in the info object.

For an example, I am going to connect to the endpoint with telnet:

```
Joshs-MacBook-Pro:~ Josh$ telnet 202.101.228.108 1494
 Trying 202.101.228.108...
 Connected to 202.101.228.108.
 Escape character is '^]'.
 hello
 aaaaaaaaa
```

Note that this connection to the newly bound port does not need to go through the SOCKS handshake.

Back at our original connection we see that we have received some new data:

```
8
<Buffer 00 5a ca 61 43 a8 09 01>  // This first piece of information can be ignored.

7
<Buffer 68 65 6c 6c 6f 0d 0a> // Hello <\r\n (enter key)>

11
<Buffer 61 61 61 61 61 61 61 61 61 0d 0a> // aaaaaaaaa <\r\n (enter key)>
```

As you can see the data entered in the telnet terminal is routed through the SOCKS proxy and back to the original connection that was made to the proxy.

**Note** Please pay close attention to the first piece of data that was received.

```
<Buffer 00 5a ca 61 43 a8 09 01>

        [005a] [PORT:2} [IP:4]
```

This piece of data is technically part of the SOCKS BIND specifications, but because of my design decisions that were made in an effort to keep this library simple to use, you will need to make sure to ignore and/or deal with this initial packet that is received when a connection is made to the newly opened port.

### Associate Example:
The associate command sets up a UDP relay for the remote SOCKS proxy server to relay UDP packets to the remote host of your choice.

```javascript
var options = {
    proxy: {
        ipaddress: "202.101.228.108",
        port: 1080,
        type: 5,
        command: "associate" // Since we are using associate, we must specify it here.
    },
    target: {
        // When using associate, either set the ip and port to 0.0.0.0:0 or the expected source of incoming udp packets.
        // Note: Some SOCKS servers MAY block associate requests with 0.0.0.0:0 endpoints.
        // Note: ipv4, ipv6, and hostnames are supported here.
        host: "0.0.0.0",
        port: 0
    }
};


Socks.createConnection(options, function(err, socket, info) {
    if (err)
        console.log(err);
    else {
        // Associate request has completed.
        // info object contains the remote ip and udp port to send UDP packets to.
        console.log(info);
        // { port: 42803, host: '202.101.228.108' }

        var udp = new dgram.Socket('udp4');

        // In this example we are going to send "Hello" to 1.2.3.4:2323 through the SOCKS proxy.

        var pack = Socks.createUDPFrame({ host: "1.2.3.4", port: 2323}, new Buffer("hello"));

        // Send Packet to Proxy UDP endpoint given in the info object.
        udp.send(pack, 0, pack.length, info.port, info.host);
    }
});

```
Now assuming that the associate request went through correctly. Anything that is typed in the stdin will first be sent to the SOCKS proxy on the endpoint that was provided in the info object. Once the SOCKS proxy receives it, it will then forward on the actual UDP packet to the host you you wanted.


1.2.3.4:2323 should now receive our relayed UDP packet from 202.101.228.108 (SOCKS proxy)
```
// <Buffer 68 65 6c 6c 6f>
```

## Using socks as an HTTP Agent

You can use socks as a http agent which will relay all your http
connections through the socks server.

The object that `Socks.Agent` accepts is the same as `Socks.createConnection`, you don't need to set a target since you have to define it in `http.request` or `http.get` methods.

The second argument is a boolean which indicates whether the remote endpoint requires TLS.

```javascript
var socksAgent = new Socks.Agent({
    proxy: {
        ipaddress: "202.101.228.108",
        port: 1080,
        type: 5,
    }},
    true, // we are connecting to a HTTPS server, false for HTTP server
    false // rejectUnauthorized option passed to tls.connect(). Only when secure is set to true
);

http.get({ hostname: 'google.com', port: '443', agent: socksAgent}, function (res) {
    // Connection header by default is keep-alive, we have to manually end the socket
    socksAgent.encryptedSocket.end();
});
```

# Api Reference:

There are only three exported functions that you will ever need to use.

### Socks.createConnection( options, callback(err, socket, info)  )
> `Object` **Object containing options to use when creating this connection**

> `function` **Callback that is called when connection completes or errors**

Options:

```javascript
var options = {

    // Information about proxy server
    proxy: {
        // IP Address of Proxy (Required)
        ipaddress: "1.2.3.4",

        // TCP Port of Proxy (Required)
        port: 1080,

        // Proxy Type [4, 5] (Required)
        // Note: 4 works for both 4 and 4a.
        type: 4,

        // SOCKS Connection Type (Optional)
        // - defaults to 'connect'

        // 'connect'    - establishes a regular SOCKS connection to the target host.
        // 'bind'       - establishes an open tcp port on the SOCKS for another client to connect to.
        // 'associate'  - establishes a udp association relay on the SOCKS server.
        command: "connect",


        // SOCKS 4 Specific:

        // UserId used when making a SOCKS 4/4a request. (Optional)
        userid: "someuserid",

        // SOCKS 5 Specific:

        // Authentication used for SOCKS 5 (when it's required) (Optional)
        authentication: {
            username: "Josh",
            password: "somepassword"
        }
    },

    // Information about target host and/or expected client of a bind association. (Required)
    target: {
        // When using 'connect':    IP Address or hostname (4a and 5 only) of a target to connect to.
        // When using 'bind':       IP Address of the expected client that will connect to the newly open tcp port.
        // When using 'associate':  IP Address and Port of the expected client that will send UDP packets to this UDP association relay.

        // Note:
        // When using SOCKS 4, only an ipv4 address can be used.
        // When using SOCKS 4a, an ipv4 address OR a hostname can be used.
        // When using SOCKS 5, ipv4, ipv6, or a hostname can be used.
        host: "1.2.3.4",

        // TCP port of target to connect to.
        port: 1080
    },

    // Amount of time to wait for a connection to be established. (Optional)
    // - defaults to 10000ms (10 seconds)
    timeout: 10000
};
```
Callback:

```javascript

// err:  If an error occurs, err will be an Error object, otherwise null.
// socket: Socket with established connection to your target host.
// info: If using BIND or associate, this will be the remote endpoint to use.

function(err, socket, info) {
  // Hopefully no errors :-)
}
```

### Socks.createUDPFrame( target, data, [frame] )
> `Object` **Target host object containing destination for UDP packet**

> `Buffer` **Data Buffer to send in the UDP packet**

> `Number` **Frame number in UDP packet. (defaults to 0)**

Creates a UDP packet frame for using with UDP association relays.

returns `Buffer` The completed UDP packet container to be sent to the proxy for forwarding.

target:
```javascript

// Target host information for where the UDP packet should be sent.
var target =
    {
        // ipv4, ipv6, or hostname for where to have the proxy send the UDP packet.
        host: "1.2.3.4",

        // udpport for where to send the UDP packet.
        port: 2323
    }

```

### Socks.Agent( options, tls)  )
> `Object` **Object containing options to use when creating this connection (see above in createConnection)**

> `boolean` **Boolean indicating if we upgrade the connection to TLS on the socks server**


# Further Reading:
Please read the SOCKS 5 specifications for more information on how to use BIND and Associate.
http://www.ietf.org/rfc/rfc1928.txt

# License
This work is licensed under the [MIT license](http://en.wikipedia.org/wiki/MIT_License).
