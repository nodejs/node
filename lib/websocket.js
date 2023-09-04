
const crypto = require('node:crypto');
const net = require('node:net');
const os = require('node:os');
const tls = require('node:tls');


/**
 * ```typescript
 * interface extensions {
 *     // A callback that fires once the connection handshake completes.
 *   callbackOpen?: (
 *     err:NodeJS.ErrnoException,
 *     socket:Socket|TLSSocket
 *   ) => void;
 *     // Extensions are the place to provide custom identifiers, security
 *     // tokens, and other descriptors.
 *   extensions?: string;
 *     // eliminates use of message masking as required by default for
 *     // client-side sockets.
 *     // Default: `true`
 *   masking?: boolean;
 *     // Arbitrary unique identifier of user's choosing.
 *   messageHandler?: (message:Buffer) => void;
 *     // If connecting through a proxy that requires an authentication token.
 *   proxy-authorization?: string;
 *     // A comma separated list of one or more known subprotocol values per
 *     // RFC 6455 11.2.
 *   subProtocol?: string;
 * }
 * interface clientOptions extends extensions {
 *     // Whether to create a Socket or TLSSocket type socket. Defaults to
 *     // `true`
 *   secure?: boolean;
 *     // Required options to create a Node socket.
 *   socketOptions: net.NetConnectOpts | tls.ConnectionOptions;
 * }
 * (options:clientOptions) => Socket | TLSSocket;
 * ``` */

// creates and connects a websocket client
function clientConnect(options) {
  const socket = (options.secure === false) ?
    net.connect(options.socketOptions) :
    tls.connect(options.socketOptions);
  // RFC 6455 1.7 says the only relation to HTTP is that a valid handshake
  // be sent and received in the form of a HTTP 1.1 GET header, so this
  // application will send the conforming header text and otherwise avoid
  // the overhead of HTTP, which will greatly boost execution performance.
  function handlerReady() {
    // response from server
    socket.once('data', function(responseData) {
      const hash = crypto.createHash('sha1');
      const responseString = responseData.toString();
      const response = responseString.replace(/\r\n/g, '\n').split('\n');
      const len = response.length;
      function callbackCheck() {
        if (typeof options.callbackOpen === 'function') {
          options.callbackOpen(errorObject(
            'ECONNABORTED',
            'WebSocket server handshake response returned no ' +
              'Sec-WebSocket-Accept header or one which is malformed.',
            socket,
            'websocket.clientConnect'
          ), null);
        }
      }
      // check if response contains required HTTP header
      if (len > 1 && responseString.includes('HTTP/1.1 101 Switching Protocols')) {
        let index = 0;
        let key = '';
        let colon = 0;
        do {
          colon = response[index].indexOf(':');
          // find the header dealing with the handshake
          if (response[index].slice(0, colon).toLowerCase() === 'sec-websocket-accept') {
            key = response[index].slice(colon + 1).replace(/^\s+/, '').replace(/\s+$/, '');
            break;
          }
          index = index + 1;
        } while (index < len);
        hash.update(nonceSource + websocket.magicString);
        const digest = hash.digest('hex');//console.log(digest);console.log(key);
        // validate handshake
        if (digest === key) {
          delete options.secure;
          delete options.socketOptions;
          socketExtend(socket, options);
        } else {
          callbackCheck();
        }
      } else {
        callbackCheck();
      }
    });
    const addresses = getAddress(socket);
    const resourceName = (typeof options.socketOptions.host === 'string') ?
      (function() {
        const host = options.socketOptions.host;
        const scheme = (/^\w+:\/\//);
        if (typeof host !== 'string') {
          return '/';
        }
        if (scheme.test(host) === false) {
          return '/';
        }
        host.replace(scheme, '');
        if (host.indexOf('/') < 0 || host.indexOf('/') === host.length - 1) {
          return '/';
        }
        return `/${host.slice(host.indexOf('/') + 1)}`;
      }()) :
      '/';
    const nonceSource = Buffer.from(Date.now().toString() + os.hostname()).toString('base64');
    const header = [
      `GET ${resourceName} HTTP/1.1`,
      (addresses.remote.address.indexOf(':') > -1) ?
        // RFC 6455 4.1. request host be defined according to RFC 3986 (URI)
        // however it also requires the socket be already established and open
        // at layer 4 (TCP/TLS) and layer 4 cares only for IP address as URI
        // is layer 7. For sanity ip/port is fully sufficient if derived from
        // the established socket
        `Host: [${addresses.remote.address}]:${addresses.remote.port}` :
        `Host: ${addresses.remote.address}:${addresses.remote.port}`,
      'Upgrade: websocket',
      'Connection: Upgrade',
      'Sec-WebSocket-Version: 13',
      `Sec-WebSocket-Key: ${nonceSource}`,
      `User-Agent: Node.js--${process.version}--${os.version()}--${os.release()}`
    ];
    function headingCheck(heading) {
      if (typeof options[heading] === 'string') {
        if (heading === 'extensions') {
          header.push(`Sec-WebSocket-Extensions: ${options.extensions}`);
        } else if (heading === 'subProtocol') {
          header.push(`Sec-WebSocket-Protocol: ${options[heading]}`);
        } else {
          header.push(`Proxy-Authorization: ${options[heading]}`);
        }
      }
    }
    headingCheck('extensions');
    headingCheck('proxy-authorization');
    headingCheck('subProtocol');
    options.role = 'client';

    header.push('');
    header.push('');
    // last use of socket.write before its hidden in socket extensions
    socket.write(header.join('\r\n'));
  }

  // prevents a crashing server from breaking the client node instance
  socket.on('error', function() {});

  // wait for layer 4 socket connection
  socket.once('ready', handlerReady);
  return socket;
}

// a uniform convenience function for generating error objects
function errorObject(code, message, socket, syscall) {
  const addresses = getAddress(socket).remote;
  return new Error({
    address: addresses.address,
    code: code,
    errno: 0,
    message: message,
    port: addresses.port,
    syscall: syscall
  });
}

// a handy utility to conveniently gather a socket's connection identity
function getAddress(socket) {
  function parse(input) {
    if (input === undefined) {
      return 'undefined, possibly due to socket closing';
    }
    if (input.indexOf('::ffff:') === 0) {
      return input.replace('::ffff:', '');
    }
    if (input.indexOf(':') > 0 && input.indexOf('.') > 0) {
      return input.slice(0, input.lastIndexOf(':'));
    }
    return input;
  }
  return {
    local: {
      address: parse(socket.localAddress),
      port: socket.localPort
    },
    remote: {
      address: parse(socket.remoteAddress),
      port: socket.remotePort
    }
  };
}

// send a message payload in conformance to RFC 6455
function messageSend(message, opcode, fragmentSize) {
  const socket = this;
  function writeFrame() {
    function writeCallback() {
      socket.websocket.queue.splice(0, 1);
      if (socket.websocket.queue.length > 0) {
        writeFrame();
      } else {
        socket.websocket.status = 'open';
      }
    };
    if (socket.websocket.status === 'open') {
      socket.websocket.status = 'pending';
    }
    if (socket.internalWrite(socket.websocket.queue[0]) === true) {
      writeCallback();
    } else {
      socket.once('drain', writeCallback);
    }
  };
  function mask(body) {
    const mask = Buffer.alloc(4);
    const rand = Buffer.from(Math.random().toString());
    mask[0] = rand[4];
    mask[1] = rand[5];
    mask[2] = rand[6];
    mask[3] = rand[7];
    // RFC 6455, 5.3.  Client-to-Server Masking
    // j                   = i MOD 4
    // transformed-octet-i = original-octet-i XOR masking-key-octet-j
    body.forEach(function(value, index) {
      body[index] = value ^ mask[index % 4];
    });
    return [body, mask];
  }
  // OPCODES
  // ## Messages
  // 0 - continuation - fragments of a message payload following an initial
  //     fragment
  // 1 - text message
  // 2 - binary message
  // 3-7 - reserved for future use
  //
  // ## Control Frames
  // 8 - close, the remote is destroying the socket
  // 9 - ping, a connectivity health check
  // a - pong, a response to a ping
  // b-f - reserved for future use
  //
  // ## Notes
  // * Message frame fragments must be transmitted in order and not interleaved
  //   with other messages.
  // * Message types may be supplied as buffer or socketData types, but will
  //   always be transmitted as buffers.
  // * Control frames are always granted priority and may occur between fragments
  //   of a single message.
  // * Control frames will always be supplied as buffer data types.
  //
  // ## Masking
  // * All traffic coming from the browser will be websocket masked.
  // * I have not tested if the browsers will process masked data as they
  //   shouldn't according to RFC 6455.
  // * This application supports both masked and unmasked transmission so long
  //   as the mask bit is set and a 32bit mask key is supplied.
  // * Mask bit is set as payload length (up to 127) + 128 assigned to frame
  //   header second byte.
  // * Mask key is first 4 bytes following payload length bytes (if any).
  if (typeof opcode !== 'number') {
    opcode = 1;
  } else {
    opcode = Math.floor(opcode);
    if (opcode < 0 || opcode > 15) {
      opcode = 1;
    }
  }
  if (opcode === 1 && Buffer.isBuffer(message) === true) {
    opcode = 2;
  }
  if (typeof fragmentSize !== 'number' || fragmentSize < 1) {
    fragmentSize = 0;
  }
  if (opcode === 1 || opcode === 2 || opcode === 3 || opcode === 4 || opcode === 5 || opcode === 6 || opcode === 7) {
    let maskKey = null;
    function fragmentation(first) {
      let finish = false;
      const frameBody = (function() {
        if (fragmentSize < 1 || len === fragmentSize) {
          finish = true;
          if (socket.websocket.masking === true) {
            const masked = mask(dataPackage);
            maskKey = masked[1];
            return masked[0];
          }
          return dataPackage;
        }
        const fragment = dataPackage.subarray(0, fragmentSize);
        dataPackage = dataPackage.subarray(fragmentSize);
        len = dataPackage.length;
        if (len < fragmentSize) {
          finish = true;
        }
        if (socket.websocket.masking === true) {
          const masked = mask(fragment);
          maskKey = masked[1];
          return masked[0];
        }
        return fragment;
      }());
      const size = frameBody.length;
      const frameHeader = (function() {
        // frame 0 is:
        // * 128 bits for fin, 0 for unfinished plus opcode
        // * opcode 0 - continuation of fragments
        // * opcode 1 - text (total payload must be UTF8 and probably not contain hidden
        //              control characters)
        // * opcode 2 - supposed to be binary, really anything that isn't 100& UTF8 text
        // ** for fragmented data only first data frame gets a data opcode, others
        //              receive 0 (continuity)
        const frame = (size < 126) ?
          (socket.websocket.masking === true) ?
            Buffer.alloc(6) :
            Buffer.alloc(2) :
          (size < 65536) ?
            (socket.websocket.masking === true) ?
              Buffer.alloc(8) :
              Buffer.alloc(4) :
            (socket.websocket.masking === true) ?
              Buffer.alloc(14) :
              Buffer.alloc(10);
        frame[0] = (finish === true) ?
          (first === true) ?
            128 + opcode :
            128 :
          (first === true) ?
            opcode :
            0;
        // frame 1 is mask bit + length flag
        frame[1] = (size < 126) ?
          (socket.websocket.masking === true) ?
            size + 128 :
            size :
          (size < 65536) ?
            (socket.websocket.masking === true) ?
              254 :
              126 :
            (socket.websocket.masking === true) ?
              255 :
              127;
        // write payload length followed by mask key
        if (size > 125) {
          if (size < 65536) {
            frame.writeUInt16BE(size, 2);
            if (socket.websocket.masking === true) {
              frame[6] = maskKey[0];
              frame[7] = maskKey[1];
              frame[8] = maskKey[2];
              frame[9] = maskKey[3];
            }
          } else {
            frame.writeUIntBE(size, 4, 6);
            if (socket.websocket.masking === true) {
              frame[10] = maskKey[0];
              frame[11] = maskKey[1];
              frame[12] = maskKey[2];
              frame[13] = maskKey[3];
            }
          }
        } else if (socket.websocket.masking === true) {
          frame[2] = maskKey[0];
          frame[3] = maskKey[1];
          frame[4] = maskKey[2];
          frame[5] = maskKey[3];
        }
        return frame;
      }());
      socket.websocket.queue.push(Buffer.concat([frameHeader, frameBody]));
      if (finish === true) {
        if (socket.websocket.status === 'open') {
          writeFrame();
        }
      } else {
        fragmentation(false);
      }
    };
    let dataPackage = (Buffer.isBuffer === true) ?
      message :
      Buffer.from(message);
    let len = dataPackage.length;
    fragmentation(true);
  } else if (opcode === 8 || opcode === 9 || opcode === 10 || opcode === 11 || opcode === 12 || opcode === 13 || opcode === 14 || opcode === 15) {
    let frameBody = message.subarray(0, 125);
    let frameHeader = null;
    if (socket.websocket.masking === true) {
      const masked = mask(frameBody);
      frameHeader = Buffer.alloc(6);
      // opcode + fin bit, rsv bits set to 0
      frameHeader[0] = 128 + opcode;
      // set the mask bit
      frameHeader[1] = 128 + frameBody.length;
      // set the mask key
      frameHeader[2] = masked[1][0];
      frameHeader[3] = masked[1][1];
      frameHeader[4] = masked[1][2];
      frameHeader[5] = masked[1][3];
      // assign the masked payload
      frameBody = masked[0];
    } else {
      frameHeader = Buffer.alloc(2);
      // opcode + fin bit, rsv bits set to 0
      frameHeader[0] = 128 + opcode;
      frameHeader[1] = frameBody.length;
    }
    // control frames send immediately, out of sequence
    socket.websocket.queue.unshift(Buffer.concat([frameHeader, frameBody]));

    if (socket.websocket.status === 'open') {
      writeFrame();
    }
  }
}

// arbitrary ping function, which may be called by any means at any time
function ping(ttl, callback) {
  const socket = this;
  function errorObject(code, message) {
    const err = new Error();
    err.code = code;
    err.message = `${message} Socket type ${config.socket.type} and name ${name}.`;
    return err;
  }
  if (socket.websocket.status !== 'open' && socket.websocket.status !== 'pending') {
    callback(errorObject(
      'ECONNABORTED',
      'Ping error on websocket without \'open\' or \'pending\' status.',
      socket,
      'websocket.ping'
    ), null);
  } else {
    const nameSlice = socket.hash.slice(0, 125);
    // send ping
    transmit_ws.queue(Buffer.from(nameSlice), config.socket, 9);
    socket.pong = {
        callback: callback,
        start: process.hrtime.bigint(),
        timeOut: setTimeout(function() {
            callback(socket.websocket.pong.timeOutMessage, null);
            delete socket.websocket.pong;
        }, ttl),
        timeOutMessage: errorObject('ETIMEDOUT', 'Ping timeout on websocket.'),
        ttl: BigInt(ttl * 1e6)
    };
  }
}

// processes incoming messages
function receive(socket) {
  function processor(buf) {
    //    RFC 6455, 5.2.  Base Framing Protocol
    //     0                   1                   2                   3
    //     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //    +-+-+-+-+-------+-+-------------+-------------------------------+
    //    |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
    //    |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
    //    |N|V|V|V|       |S|             |   (if payload len==126/127)   |
    //    | |1|2|3|       |K|             |                               |
    //    +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
    //    |     Extended payload length continued, if payload len == 127  |
    //    + - - - - - - - - - - - - - - - +-------------------------------+
    //    |                               |Masking-key, if MASK set to 1  |
    //    +-------------------------------+-------------------------------+
    //    | Masking-key (continued)       |          Payload Data         |
    //    +-------------------------------- - - - - - - - - - - - - - - - +
    //    :                     Payload Data continued ...                :
    //    + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
    //    |                     Payload Data continued ...                |
    //    +---------------------------------------------------------------+

    function unmask(input) {
      if (frame.mask === true) {
        // RFC 6455, 5.3.  Client-to-Server Masking
        // j                   = i MOD 4
        // transformed-octet-i = original-octet-i XOR masking-key-octet-j
        input.forEach(function(value, index) {
          input[index] = value ^ frame.maskKey[index % 4];
        });
      }
      return input;
    };
    // identify payload extended length
    const extended = function(input) {
      const mask = (input[1] > 127),
        len = (mask === true) ?
          input[1] - 128 :
          input[1],
        keyOffset = (mask === true) ?
          4 :
          0;
      if (len < 126) {
        return {
          lengthExtended: len,
          lengthShort: len,
          mask: mask,
          startByte: 2 + keyOffset
        };
      }
      if (len < 127) {
        return {
          lengthExtended: input.subarray(2, 4).readUInt16BE(0),
          lengthShort: len,
          mask: mask,
          startByte: 4 + keyOffset
        };
      }
      return {
        lengthExtended: input.subarray(4, 10).readUIntBE(0, 6),
        lengthShort: len,
        mask: mask,
        startByte: 10 + keyOffset
      };
    };
    // populates data from the incoming network buffer with no assumptions of
    // completeness
    const data = (function() {
      if (buf !== null && buf !== undefined) {
        socket.websocket.frame = Buffer.concat([socket.websocket.frame, buf]);
      }
      if (socket.websocket.frame.length < 2) {
        return null;
      }
      return socket.websocket.frame;
    }());
    // interprets the frame header from Buffer to an object
    const frame = (function() {
      if (data === null) {
        return null;
      }
      // bit string - convert byte number (0 - 255) to 8 bits
      const bits0 = data[0].toString(2).padStart(8, '0');
      const meta = extended(data);
      return {
        fin: (data[0] > 127),
        rsv1: (bits0.charAt(1) === '1'),
        rsv2: (bits0.charAt(2) === '1'),
        rsv3: (bits0.charAt(3) === '1'),
        opcode: (
          (Number(bits0.charAt(4)) * 8) +
          (Number(bits0.charAt(5)) * 4) +
          (Number(bits0.charAt(6)) * 2) +
          Number(bits0.charAt(7))
        ),
        mask: meta.mask,
        len: meta.lengthShort,
        extended: meta.lengthExtended,
        maskKey: (meta.mask === true) ?
          data.subarray(meta.startByte - 4, meta.startByte) :
          null,
        startByte: meta.startByte
      };
    }());
    const payload = (function() {
      // Payload processing must contend with these 4 constraints:
      // 1. Message Fragmentation - RFC6455 allows messages to be fragmented from a
      //                            single transmission into multiple transmission
      //                            frames independently sent and received.
      // 2. Header Separation     - Firefox sends frame headers separated from frame
      //                            bodies.
      // 3. Node Concatenation    - If Node.js receives message frames too quickly the
      //                            various binary buffers are concatenated into a
      //                            single deliverable to the processing application.
      // 4. TLS Max Packet Size   - TLS forces a maximum payload size of 65536 bytes.
      if (frame === null) {
        return null;
      }
      let complete = null;
      const size = frame.extended + frame.startByte;
      const len = socket.websocket.frame.length;
      if (len < size) {
        return null;
      }
      complete = unmask(socket.websocket.frame.subarray(frame.startByte, size));
      socket.websocket.frame = socket.websocket.frame.subarray(size);

      return complete;
    }());

    if (payload === null) {
      return;
    }

    if (frame.opcode === 8) {
      // socket close
      data[0] = 136;
      data[1] = (data[1] > 127) ?
        data[1] - 128 :
        data[1];
      const payload = Buffer.concat([data.subarray(0, 2), unmask(data.subarray(2))]);
      socket.write(payload);
      socket.off('data', processor);
    } else if (frame.opcode === 9) {
      // respond to 'ping' as 'pong'
      socket.send(data.subarray(frame.startByte), socket, 10);
    } else if (frame.opcode === 10) {
      // pong
      const payloadString = payload.toString();
      const pong = socket.websocket.pong[payloadString];
      const time = process.hrtime.bigint();
      if (pong !== undefined) {
        if (time < pong.start + pong.ttl) {
          clearTimeout(pong.timeOut);
          pong.callback(null, time - pong.start);
        }
        delete socket.websocket.pong[payloadString];
      }
    } else {
      const segment = Buffer.concat([socket.websocket.fragment, payload]);
      // this block may include frame.opcode === 0 - a continuation frame
      socket.websocket.frameExtended = frame.extended;
      if (frame.fin === true) {
        if (typeof socket.websocket.messageHandler === 'function') {
          socket.websocket.messageHandler(segment.subarray(0, socket.websocket.frameExtended));
        }
        socket.websocket.fragment = segment.subarray(socket.websocket.frameExtended);
      } else {
        socket.websocket.fragment = segment;
      }
    }
    if (socket.websocket.frame.length > 2) {
      processor(null);
    }
  };
  socket.on('data', processor);
}

/**
 * ```typescript
 * interface serverOptions {
 *     // Optional callback for when a socket connects to the server, before
 *     // completion of the handshake.
 *     // Insert custom socket extensions or authentication logic here.
 *     // The callbackConnect function must call `extend()` in order to
 *     // complete the connection handshake.
 *   callbackConnect?: (
 *     headerValues:headerValues,
 *     socket:Socket|TLSSocket,
 *     ready:() => void,
 *   ) => void;
 *     // Optional callback for when server begins to listen for sockets.
 *   callbackListener?: (server:Server) => void;
 *     // Optional callback that fires once the connection handshake completes.
 *   callbackOpen?: (err:NodeJS.ErrnoException, socket:Socket|TLSSocket) => void;
 *     // A handler to receive processed messages. If not a function incoming
 *     // messages are ignored.
 *   messageHandler?: (message:Buffer) => void;
 *     // Options for the server's listener event emitter.
 *   listenerOptions: ListenerOptions;
 *     // Whether to invoke tls.createServer or net.createServer.
 *   secure?: boolean;
 *     // Node core options object for net.createServer or tls.createServer.
 *   serverOptions?: ServerOpts | TlsOptions;
 * }
 * (options:serverOptions) => Server;
 * ``` */
// creates a websocket server
function server(options) {
  function connection(socket) {
    // prevents a closing socket from crashing the server
    socket.on('error', function () {});
    // socket handshake must be processed within 5 seconds of connection
    // this is a security precaution to prevent overloading servers
    const deathDelay = setTimeout(function() {
      socket.destroy();
    }, 5000);
    // the first data must be the HTTP handshake, otherwise destroy
    socket.once('data', function(data) {
      // we expect data to be the handshake payload in HTTP form
      const headerValues = {
        authentication: '',
        id: '',
        key: '',
        subprotocol: '',
        type: '',
        userAgent: '',
      };
      const dataString = data.toString();
      const lowString = dataString.toLowerCase();
      const headerList = dataString.split('\r\n');
      const flags = {
        complete: false,
        extensions: (lowString.includes('\r\nsec-websocket-extensions:')) ?
          false :
          true,
        key: false,
        subprotocol: (lowString.includes('\r\nsec-webSocket-protocol:')) ?
          false :
          true,
        userAgent: (lowString.includes('\r\nuser-agent:')) ?
          false :
          true,
      };
      function complete() {
        if (
          flags.authentication === true &&
          flags.extensions === true &&
          flags.subprotocol === true &&
          flags.userAgent === true &&
          flags.complete === false
        ) {
          function extend() {
            // send the handshake response
            const headers = [
              'HTTP/1.1 101 Switching Protocols',
              'Upgrade: websocket',
              'Connection: Upgrade',
              `Sec-WebSocket-Accept: ${headerValues.key}`,
              `Sec-WebSocket-Protocol: ${headerValues
                .subprotocol.split(',')[0]
                .replace(/^\s+/, '')
                .replace(/\s+$/, '')
              }`,
            ];
            headers.push('');
            headers.push('');
            socket.write(headers.join('\r\n'));

            // extend the socket
            socketExtend(socket, {
              callbackOpen: options.callbackOpen,
              extensions: headerValues.extensions,
              messageHandler: options.messageHandler,
              'proxy-authorization': '',
              'role': 'server',
              subprotocol: headerValues.subprotocol,
              userAgent: headerValues.userAgent,
            });
          }
          flags.complete = true;
          clearTimeout(deathDelay);
          if (typeof options.callbackConnect === 'function') {
            options.callbackConnect(headerValues, socket, extend);
          } else {
            extend();
          }
        }
      }
      headerList.forEach(function(header) {
        const index = header.indexOf(':');
        const lowHeader = header.toLowerCase().slice(0, index);
        const value = header.slice(index + 1).replace(/^\s+/, '');
        if (lowHeader === 'sec-websocket-key') {
          const hash = crypto.createHash('sha1');
          const key = value + websocket.magicString;
          hash.update(key);
          headerValues.key = hash.digest('hex');
          flags.key = true;
        } else if (lowHeader === 'sec-webSocket-protocol') {
          headerValues.subprotocol = value;
          flags.subprotocol = true;
        } else if (lowHeader === 'sec-websocket-extensions') {
          headerValues.extensions = value;
          flags.extensions = true;
        } else if (lowHeader === 'user-agent') {
          headerValues.userAgent = value;
          flags.userAgent = true;
        }
        complete();
      });
      if (flags.complete === false) {
        socket.destroy();
        clearTimeout(deathDelay);
      }
    });
  }
  const serverInstance = (options.secure === false) ?
    net.createServer(options.serverOptions) :
    tls.createServer(options.serverOptions, connection);
  if (options.secure === false) {
    serverInstance.on('connection', connection);
  }
  serverInstance.listen(options.listenerOptions, function () {
    if (typeof options.callbackListener === 'function') {
      options.callbackListener(serverInstance);
    }
  });
  return serverInstance;
}

// extends a net Socket type with additional features specific for WebSocket
function socketExtend(socket, extensions) {
  function stringExtensions(name) {
    socket.websocket[name] = (typeof extensions[name] === 'string') ?
      extensions[name] :
      '';
  }
  socket.websocket = (
    extensions === null ||
    extensions === undefined ||
    typeof extensions !== 'object'
  ) ?
    {} :
    extensions;
  if (typeof socket.websocket.messageHandler === 'function') {
    receive(socket);
  }
  // arbitrary ping utility
  socket.ping = ping;
  // send a message
  socket.messageSend = messageSend;
  socket.setKeepAlive(true, 0);
  stringExtensions('extensions');
  stringExtensions('proxy-authorization');
  stringExtensions('role');
  stringExtensions('userAgent');
  socket.websocket.masking = (typeof extensions.masking === 'boolean') ?
    extensions.masking :
    extensions.role === 'client' ?
      true :
      false;
  // storehouse of complete received data frames
  // a frame is a message fragment considering for continuation frames
  socket.websocket.fragment = Buffer.from([]);
  // stores pieces of frames for assembly into complete frames
  socket.websocket.frame = Buffer.from([]);
  // stores the payload size of current received message payload
  socket.websocket.frameExtended = 0;
  // stores termination times and callbacks for pong handling
  socket.websocket.pong = {};
  // stores messages for transmit in order,
  // because websocket protocol cannot intermix messages
  socket.websocket.queue = [];
  socket.websocket.status = 'open';
  // hides the generic socket.write method to encourage use of messageSend
  socket.internalWrite = socket.write;
  socket.write = messageSend;
  if (typeof extensions.callbackOpen === 'function') {
    extensions.callbackOpen(null, socket);
  }
}

module.exports = websocket = {
  clientConnect,
  getAddress,
  magicString: '258EAFA5-E914-47DA-95CA-C5AB0DC85B11',
  server: server
};