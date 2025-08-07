'use strict';
const common = require('./index');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http = require('http');
const crypto = require('crypto');

class WebSocketServer {
  constructor({
    port = 0,
    responseError = false,
  }) {
    this.port = port;
    this.server = http.createServer();
    this.clients = new Set();
    this.responseError = responseError;

    this.server.on('upgrade', this.handleUpgrade.bind(this));
  }

  start() {
    return new Promise((resolve) => {
      this.server.listen(this.port, () => {
        this.port = this.server.address().port;
        resolve();
      });
    }).catch((err) => {
      console.error('Failed to start WebSocket server:', err);
    });
  }

  handleUpgrade(req, socket, head) {
    const key = req.headers['sec-websocket-key'];
    const acceptKey = this.generateAcceptValue(key);
    const responseHeaders = [
      'HTTP/1.1 101 Switching Protocols',
      'Upgrade: websocket',
      'Connection: Upgrade',
      `Sec-WebSocket-Accept: ${acceptKey}`,
    ];

    socket.write(responseHeaders.join('\r\n') + '\r\n\r\n');
    this.clients.add(socket);

    socket.on('data', (buffer) => {
      const opcode = buffer[0] & 0x0f;

      if (opcode === 0x8) {
        socket.end();
        this.clients.delete(socket);
        return;
      }

      if (this.responseError) {
        socket.write(Buffer.from([0x88, 0x00])); // close frame
        socket.end();
        this.clients.delete(socket);
        return;
      }
      socket.write(this.encodeMessage('Hello from server!'));
    });

    socket.on('close', () => {
      this.clients.delete(socket);
    });

    socket.on('error', (err) => {
      console.error('Socket error:', err);
      this.clients.delete(socket);
    });
  }

  generateAcceptValue(secWebSocketKey) {
    return crypto
      .createHash('sha1')
      .update(secWebSocketKey + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11', 'binary')
      .digest('base64');
  }

  decodeMessage(buffer) {
    const secondByte = buffer[1];
    const length = secondByte & 127;
    const maskStart = 2;
    const dataStart = maskStart + 4;
    const masks = buffer.slice(maskStart, dataStart);
    const data = buffer.slice(dataStart, dataStart + length);
    const result = Buffer.alloc(length);

    for (let i = 0; i < length; i++) {
      result[i] = data[i] ^ masks[i % 4];
    }

    return result.toString();
  }

  encodeMessage(message) {
    const msgBuffer = Buffer.from(message);
    const length = msgBuffer.length;
    const frame = [0x81];

    if (length < 126) {
      frame.push(length);
    } else if (length < 65536) {
      frame.push(126, (length >> 8) & 0xff, length & 0xff);
    } else {
      throw new Error('Message too long');
    }

    return Buffer.concat([Buffer.from(frame), msgBuffer]);
  }
}

module.exports = WebSocketServer;
