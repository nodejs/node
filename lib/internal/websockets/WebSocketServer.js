'use strict'
/*!
 * ws: a node.js websocket client
 * Copyright(c) 2011 Einar Otto Stangvik <einaros@gmail.com>
 * MIT Licensed
 */

const util = require('util');
const events = require('events');
const http = require('http');
const crypto = require('crypto');
const WebSocket = require('./WebSocket');
const Extensions = require('./Extensions');
const PerMessageDeflate = require('./PerMessageDeflate');
const tls = require('tls');
const url = require('url');

/**
 * utils
 */

/**
* Entirely private apis,
* which may or may not be bound to a sepcific WebSocket instance.
*/
function handleHybiUpgrade(req, socket, upgradeHead, cb) {
 // handle premature socket errors
 let errorHandler = function() {
   try { socket.destroy(); } catch (e) {}
 }
 socket.on('error', errorHandler);

 // verify key presence
 if (!req.headers['sec-websocket-key']) {
   abortConnection(socket, 400, 'Bad Request');
   return;
 }

 // verify version
 let version = parseInt(req.headers['sec-websocket-version']);
 if ([8, 13].indexOf(version) === -1) {
   abortConnection(socket, 400, 'Bad Request');
   return;
 }

 // verify protocol
 let protocols = req.headers['sec-websocket-protocol'];

 // verify client
 let origin = version < 13 ?
   req.headers['sec-websocket-origin'] :
   req.headers['origin'];

 // handle extensions offer
 let extensionsOffer = Extensions.parse(req.headers['sec-websocket-extensions']);

 // handler to call when the connection sequence completes
 let self = this;
 let completeHybiUpgrade2 = function(protocol) {

   // calc key
   let key = req.headers['sec-websocket-key'];
   let shasum = crypto.createHash('sha1');
   shasum.update(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
   key = shasum.digest('base64');

   let headers = [
       'HTTP/1.1 101 Switching Protocols'
     , 'Upgrade: websocket'
     , 'Connection: Upgrade'
     , 'Sec-WebSocket-Accept: ' + key
   ];

   if (typeof protocol != 'undefined') {
     headers.push('Sec-WebSocket-Protocol: ' + protocol);
   }

   let extensions = {};
   try {
     extensions = acceptExtensions.call(self, extensionsOffer);
   } catch (err) {
     abortConnection(socket, 400, 'Bad Request');
     return;
   }

   if (Object.keys(extensions).length) {
     let serverExtensions = {};
     Object.keys(extensions).forEach(function(token) {
       serverExtensions[token] = [extensions[token].params]
     });
     headers.push('Sec-WebSocket-Extensions: ' + Extensions.format(serverExtensions));
   }

   // allows external modification/inspection of handshake headers
   self.emit('headers', headers);

   socket.setTimeout(0);
   socket.setNoDelay(true);
   try {
     socket.write(headers.concat('', '').join('\r\n'));
   }
   catch (e) {
     // if the upgrade write fails, shut the connection down hard
     try { socket.destroy(); } catch (e) {}
     return;
   }

   let client = new WebSocket([req, socket, upgradeHead], {
     protocolVersion: version,
     protocol: protocol,
     extensions: extensions
   });

   if (self.options.clientTracking) {
     self.clients.push(client);
     client.on('close', function() {
       let index = self.clients.indexOf(client);
       if (index != -1) {
         self.clients.splice(index, 1);
       }
     });
   }

   // signal upgrade complete
   socket.removeListener('error', errorHandler);
   cb(client);
 }

 // optionally call external protocol selection handler before
 // calling completeHybiUpgrade2
 let completeHybiUpgrade1 = function() {
   // choose from the sub-protocols
   if (typeof self.options.handleProtocols == 'function') {
       let protList = (protocols || "").split(/, */);
       let cbCalled = false;
       let res = self.options.handleProtocols(protList, function(result, protocol) {
         cbCalled = true;
         if (!result) abortConnection(socket, 401, 'Unauthorized');
         else completeHybiUpgrade2(protocol);
       });
       if (!cbCalled) {
           // the handleProtocols handler never called our cb
           abortConnection(socket, 501, 'Could not process protocols');
       }
       return;
   } else {
       if (typeof protocols !== 'undefined') {
           completeHybiUpgrade2(protocols.split(/, */)[0]);
       }
       else {
           completeHybiUpgrade2();
       }
   }
 }

 // optionally call external client verification handler
 if (typeof this.options.verifyClient == 'function') {
   let info = {
     origin: origin,
     secure: typeof req.connection.authorized !== 'undefined' || typeof req.connection.encrypted !== 'undefined',
     req: req
   };
   if (this.options.verifyClient.length == 2) {
     this.options.verifyClient(info, function(result, code, name) {
       if (typeof code === 'undefined') code = 401;
       if (typeof name === 'undefined') name = http.STATUS_CODES[code];

       if (!result) abortConnection(socket, code, name);
       else completeHybiUpgrade1();
     });
     return;
   }
   else if (!this.options.verifyClient(info)) {
     abortConnection(socket, 401, 'Unauthorized');
     return;
   }
 }

 completeHybiUpgrade1();
}

function handleHixieUpgrade(req, socket, upgradeHead, cb) {
 // handle premature socket errors
 let errorHandler = function() {
   try { socket.destroy(); } catch (e) {}
 }
 socket.on('error', errorHandler);

 // bail if options prevent hixie
 if (this.options.disableHixie) {
   abortConnection(socket, 401, 'Hixie support disabled');
   return;
 }

 // verify key presence
 if (!req.headers['sec-websocket-key2']) {
   abortConnection(socket, 400, 'Bad Request');
   return;
 }

 let origin = req.headers['origin']
   , self = this;

 // setup handshake completion to run after client has been verified
 let onClientVerified = function() {
   let wshost;
   if (!req.headers['x-forwarded-host'])
       wshost = req.headers.host;
   else
       wshost = req.headers['x-forwarded-host'];
   let location = ((req.headers['x-forwarded-proto'] === 'https' || socket.encrypted) ? 'wss' : 'ws') + '://' + wshost + req.url
     , protocol = req.headers['sec-websocket-protocol'];

   // handshake completion code to run once nonce has been successfully retrieved
   let completeHandshake = function(nonce, rest) {
     // calculate key
     let k1 = req.headers['sec-websocket-key1']
       , k2 = req.headers['sec-websocket-key2']
       , md5 = crypto.createHash('md5');

     [k1, k2].forEach(function (k) {
       let n = parseInt(k.replace(/[^\d]/g, ''))
         , spaces = k.replace(/[^ ]/g, '').length;
       if (spaces === 0 || n % spaces !== 0){
         abortConnection(socket, 400, 'Bad Request');
         return;
       }
       n /= spaces;
       md5.update(String.fromCharCode(
         n >> 24 & 0xFF,
         n >> 16 & 0xFF,
         n >> 8  & 0xFF,
         n       & 0xFF));
     });
     md5.update(nonce.toString('binary'));

     let headers = [
         'HTTP/1.1 101 Switching Protocols'
       , 'Upgrade: WebSocket'
       , 'Connection: Upgrade'
       , 'Sec-WebSocket-Location: ' + location
     ];
     if (typeof protocol != 'undefined') headers.push('Sec-WebSocket-Protocol: ' + protocol);
     if (typeof origin != 'undefined') headers.push('Sec-WebSocket-Origin: ' + origin);

     socket.setTimeout(0);
     socket.setNoDelay(true);
     try {
       // merge header and hash buffer
       let headerBuffer = new Buffer(headers.concat('', '').join('\r\n'));
       let hashBuffer = new Buffer(md5.digest('binary'), 'binary');
       let handshakeBuffer = new Buffer(headerBuffer.length + hashBuffer.length);
       headerBuffer.copy(handshakeBuffer, 0);
       hashBuffer.copy(handshakeBuffer, headerBuffer.length);

       // do a single write, which - upon success - causes a new client websocket to be setup
       socket.write(handshakeBuffer, 'binary', function(err) {
         if (err) return; // do not create client if an error happens
         let client = new WebSocket([req, socket, rest], {
           protocolVersion: 'hixie-76',
           protocol: protocol
         });
         if (self.options.clientTracking) {
           self.clients.push(client);
           client.on('close', function() {
             let index = self.clients.indexOf(client);
             if (index != -1) {
               self.clients.splice(index, 1);
             }
           });
         }

         // signal upgrade complete
         socket.removeListener('error', errorHandler);
         cb(client);
       });
     }
     catch (e) {
       try { socket.destroy(); } catch (e) {}
       return;
     }
   }

   // retrieve nonce
   let nonceLength = 8;
   if (upgradeHead && upgradeHead.length >= nonceLength) {
     let nonce = upgradeHead.slice(0, nonceLength);
     let rest = upgradeHead.length > nonceLength ? upgradeHead.slice(nonceLength) : null;
     completeHandshake.call(self, nonce, rest);
   }
   else {
     // nonce not present in upgradeHead, so we must wait for enough data
     // data to arrive before continuing
     let nonce = new Buffer(nonceLength);
     upgradeHead.copy(nonce, 0);
     let received = upgradeHead.length;
     let rest = null;
     let handler = function (data) {
       let toRead = Math.min(data.length, nonceLength - received);
       if (toRead === 0) return;
       data.copy(nonce, received, 0, toRead);
       received += toRead;
       if (received == nonceLength) {
         socket.removeListener('data', handler);
         if (toRead < data.length) rest = data.slice(toRead);
         completeHandshake.call(self, nonce, rest);
       }
     }
     socket.on('data', handler);
   }
 }

 // verify client
 if (typeof this.options.verifyClient == 'function') {
   let info = {
     origin: origin,
     secure: typeof req.connection.authorized !== 'undefined' || typeof req.connection.encrypted !== 'undefined',
     req: req
   };
   if (this.options.verifyClient.length == 2) {
     let self = this;
     this.options.verifyClient(info, function(result, code, name) {
       if (typeof code === 'undefined') code = 401;
       if (typeof name === 'undefined') name = http.STATUS_CODES[code];

       if (!result) abortConnection(socket, code, name);
       else onClientVerified.apply(self);
     });
     return;
   }
   else if (!this.options.verifyClient(info)) {
     abortConnection(socket, 401, 'Unauthorized');
     return;
   }
 }

 // no client verification required
 onClientVerified();
}

function acceptExtensions(offer) {
 let extensions = {};
 let options = this.options.perMessageDeflate;
 if (options && offer[PerMessageDeflate.extensionName]) {
   let perMessageDeflate = new PerMessageDeflate(options !== true ? options : {}, true);
   perMessageDeflate.accept(offer[PerMessageDeflate.extensionName]);
   extensions[PerMessageDeflate.extensionName] = perMessageDeflate;
 }
 return extensions;
}

function abortConnection(socket, code, name) {
 try {
   let response = [
     'HTTP/1.1 ' + code + ' ' + name,
     'Content-type: text/html'
   ];
   socket.write(response.concat('', '').join('\r\n'));
 }
 catch (e) { /* ignore errors - we've aborted this connection */ }
 finally {
   // ensure that an early aborted connection is shut down completely
   try { socket.destroy(); } catch (e) {}
 }
}

/**
 * WebSocket Server implementation
 */

function WebSocketServer(options, cb) {
  if (this instanceof WebSocketServer === false) {
    return new WebSocketServer(options, cb);
  }

  events.EventEmitter.call(this);

  let opts = options

  opts.host = options.host || '0.0.0.0',
  opts.port = options.port || null,
  opts.server = options.server || null,
  opts.verifyClient = options.verifyClient || null,
  opts.handleProtocols = options.handleProtocols || null,
  opts.path = options.path || null,
  opts.noServer = options.noServer || false,
  opts.disableHixie = options.disableHixie || false,
  opts.clientTracking = options.clientTracking || true,
  opts.perMessageDeflate = options.perMessageDeflate || true

  if (!opts.port && !opts.server && !opts.noServer) {
    throw new TypeError('`port` or a `server` must be provided');
  }

  let self = this;

  if (opts.port) {
    this._server = http.createServer(function (req, res) {
      let body = http.STATUS_CODES[426];
      res.writeHead(426, {
        'Content-Length': body.length,
        'Content-Type': 'text/plain'
      });
      res.end(body);
    });
    this._server.allowHalfOpen = false;
    this._server.listen(opts.port, opts.host, cb);
    this._closeServer = function() { if (self._server) self._server.close(); };
  }
  else if (opts.server) {
    this._server = opts.server;
    if (opts.path) {
      // take note of the path, to avoid collisions when multiple websocket servers are
      // listening on the same http server
      if (this._server._webSocketPaths && opts.server._webSocketPaths[opts.path]) {
        throw new Error('two instances of WebSocketServer cannot listen on the same http server path');
      }
      if (typeof this._server._webSocketPaths !== 'object') {
        this._server._webSocketPaths = {};
      }
      this._server._webSocketPaths[opts.path] = 1;
    }
  }
  if (this._server) this._server.once('listening', function() { self.emit('listening'); });

  if (typeof this._server != 'undefined') {
    this._server.on('error', function(error) {
      self.emit('error', error)
    });
    this._server.on('upgrade', function(req, socket, upgradeHead) {
      //copy upgradeHead to avoid retention of large slab buffers used in node core
      let head = new Buffer(upgradeHead.length);
      upgradeHead.copy(head);

      self.handleUpgrade(req, socket, head, function(client) {
        self.emit('connection'+req.url, client);
        self.emit('connection', client);
      });
    });
  }

  this.options = opts;
  this.path = opts.path;
  this.clients = [];
}
util.inherits(WebSocketServer, events.EventEmitter);

/**
 * Immediately shuts down the connection.
 *
 * @api public
 */

WebSocketServer.prototype.close = function(cb) {
  // terminate all associated clients
  let error = null;
  try {
    for (let i = 0, l = this.clients.length; i < l; ++i) {
      this.clients[i].terminate();
    }
  }
  catch (e) {
    error = e;
  }

  // remove path descriptor, if any
  if (this.path && this._server._webSocketPaths) {
    delete this._server._webSocketPaths[this.path];
    if (Object.keys(this._server._webSocketPaths).length == 0) {
      delete this._server._webSocketPaths;
    }
  }

  // close the http server if it was internally created
  try {
    if (typeof this._closeServer !== 'undefined') {
      this._closeServer();
    }
  }
  finally {
    delete this._server;
  }
  if(cb)
    cb(error);
  else if(error)
    throw error;
}

/**
 * Handle a HTTP Upgrade request.
 *
 * @api public
 */
WebSocketServer.prototype.handleUpgrade = function(req, socket, upgradeHead, cb) {
  // check for wrong path
  if (this.options.path) {
    let u = url.parse(req.url);
    if (u && u.pathname !== this.options.path) return;
  }

  if (typeof req.headers.upgrade === 'undefined' || req.headers.upgrade.toLowerCase() !== 'websocket') {
    abortConnection(socket, 400, 'Bad Request');
    return;
  }

  if (req.headers['sec-websocket-key1']) handleHixieUpgrade.apply(this, arguments);
  else handleHybiUpgrade.apply(this, arguments);
}

module.exports = WebSocketServer;
