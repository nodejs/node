'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeIncludes,
  Boolean,
  NumberIsSafeInteger,
  ReflectApply,
  RegExpPrototypeExec,
  Symbol,
} = primordials;

const { Buffer } = require('buffer');
const dc = require('diagnostics_channel');
const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');

const onClientRequestUpgradeChannel = dc.channel('http.client.request.upgrade');
const onClientFrameReceivedChannel = dc.channel('websocket.client.frameReceived');
const onClientFrameSentChannel = dc.channel('websocket.client.frameSent');
const onServerRequestUpgradeChannel = dc.channel('http.server.request.upgrade');
const onServerFrameReceivedChannel = dc.channel('websocket.server.frameReceived');
const onServerFrameSentChannel = dc.channel('websocket.server.frameSent');

const kObserverState = Symbol('kObserverState');
const kWebSocketHeaderValueRegExp = /(?:^|,)\s*websocket\s*(?:,|$)/i;
const kEmptyBuffer = Buffer.alloc(0);
const kHttpHeaderTerminator = Buffer.from('\r\n\r\n');

function hasWebSocketHeader(value) {
  if (ArrayIsArray(value)) {
    for (let i = 0; i < value.length; i++) {
      if (hasWebSocketHeader(value[i])) {
        return true;
      }
    }
    return false;
  }

  return typeof value === 'string' &&
    RegExpPrototypeExec(kWebSocketHeaderValueRegExp, value) !== null;
}

function isObservableClientWebSocketUpgrade(request, response) {
  return response?.statusCode === 101 &&
    hasWebSocketHeader(response.headers?.upgrade);
}

function toBuffer(chunk, encoding) {
  if (chunk == null) {
    return null;
  }

  if (typeof chunk === 'string') {
    return Buffer.from(chunk, encoding);
  }

  if (isArrayBufferView(chunk)) {
    return Buffer.from(chunk.buffer, chunk.byteOffset, chunk.byteLength);
  }

  if (isAnyArrayBuffer(chunk)) {
    return Buffer.from(chunk);
  }

  return null;
}

function unmask(payload, mask) {
  const len = payload.length;
  let i = 0;
  for (; i + 3 < len; i += 4) {
    payload[i] ^= mask[0];
    payload[i + 1] ^= mask[1];
    payload[i + 2] ^= mask[2];
    payload[i + 3] ^= mask[3];
  }
  for (; i < len; i++) {
    payload[i] ^= mask[i & 3];
  }
}

class WebSocketFrameParser {
  constructor(onFrame) {
    this.buffer = kEmptyBuffer;
    this.onFrame = onFrame;
    this.failed = false;
  }

  execute(chunk) {
    if (this.failed) {
      return;
    }

    if (chunk.byteLength === 0) {
      return;
    }

    let buffer = this.buffer;
    if (buffer.byteLength === 0) {
      buffer = chunk;
    } else {
      buffer = Buffer.concat([buffer, chunk], buffer.byteLength + chunk.byteLength);
    }

    let offset = 0;
    while (buffer.byteLength - offset >= 2) {
      const firstByte = buffer[offset];
      const secondByte = buffer[offset + 1];

      let payloadLength = secondByte & 0x7f;
      let cursor = offset + 2;

      if (payloadLength === 126) {
        if (buffer.byteLength - offset < 4) {
          break;
        }
        payloadLength = buffer.readUInt16BE(cursor);
        cursor += 2;
      } else if (payloadLength === 127) {
        if (buffer.byteLength - offset < 10) {
          break;
        }

        const upper = buffer.readUInt32BE(cursor);
        const lower = buffer.readUInt32BE(cursor + 4);
        payloadLength = upper * 0x100000000 + lower;
        cursor += 8;

        if (!NumberIsSafeInteger(payloadLength)) {
          this.failed = true;
          this.buffer = kEmptyBuffer;
          return;
        }
      }

      let mask;
      const masked = Boolean(secondByte & 0x80);
      if (masked) {
        if (buffer.byteLength - offset < cursor - offset + 4) {
          break;
        }
        mask = buffer.subarray(cursor, cursor + 4);
        cursor += 4;
      }

      const frameEnd = cursor + payloadLength;
      if (buffer.byteLength < frameEnd) {
        break;
      }

      let payload = buffer.subarray(cursor, frameEnd);
      if (masked && payload.byteLength > 0) {
        payload = Buffer.from(payload);
        unmask(payload, mask);
      } else if (payload.byteLength === 0) {
        payload = kEmptyBuffer;
      }

      this.onFrame({
        opcode: firstByte & 0x0f,
        fin: Boolean(firstByte & 0x80),
        masked,
        compressed: Boolean(firstByte & 0x40),
        payloadLength,
        payload,
      });

      offset = frameEnd;
    }

    this.buffer = offset === buffer.byteLength ? kEmptyBuffer : buffer.subarray(offset);
  }
}

class HttpUpgradePreludeSkipper {
  constructor() {
    this.done = false;
    this.buffer = kEmptyBuffer;
  }

  execute(chunk, callback) {
    if (this.done) {
      callback(chunk);
      return;
    }

    if (this.buffer.byteLength === 0) {
      this.buffer = chunk;
    } else {
      this.buffer = Buffer.concat([this.buffer, chunk], this.buffer.byteLength + chunk.byteLength);
    }

    const headerEnd = this.buffer.indexOf(kHttpHeaderTerminator);
    if (headerEnd === -1) {
      return;
    }

    this.done = true;
    const remaining = this.buffer.subarray(headerEnd + kHttpHeaderTerminator.byteLength);
    this.buffer = kEmptyBuffer;
    if (remaining.byteLength > 0) {
      callback(remaining);
    }
  }
}

class WebSocketObserver {
  constructor({
    request,
    response,
    socket,
    primary,
    readable,
    writable,
    onFrameReceived,
    onFrameSent,
    skipHttpUpgradeResponse = false,
  }) {
    this.request = request;
    this.response = response;
    this.socket = socket;
    this.primary = primary;
    this.readable = readable;
    this.writable = writable;
    this.cleanupTargets = [];
    this.closed = false;
    this.cleanupBound = this.cleanup.bind(this);
    this.originalPush = null;
    this.originalWrite = null;

    if (typeof onFrameReceived === 'function') {
      this.receivedParser = new WebSocketFrameParser((frame) => {
        onFrameReceived({
          request: this.request,
          response: this.response,
          socket: this.socket,
          ...frame,
        });
      });
    }

    if (typeof onFrameSent === 'function') {
      this.sentParser = new WebSocketFrameParser((frame) => {
        onFrameSent({
          request: this.request,
          response: this.response,
          socket: this.socket,
          ...frame,
        });
      });
      if (skipHttpUpgradeResponse) {
        this.sentPreludeSkipper = new HttpUpgradePreludeSkipper();
      }
    }
  }

  enable(head) {
    if (this.receivedParser !== undefined && this.readable?.push) {
      this.originalPush = this.readable.push;
      const observer = this;
      this.readable.push = function push(chunk, encoding) {
        observer.observeReceivedChunk(chunk, encoding);
        return ReflectApply(observer.originalPush, this, arguments);
      };
    }

    if (this.sentParser !== undefined && this.writable?.write) {
      this.originalWrite = this.writable.write;
      const observer = this;
      this.writable.write = function write(chunk, encoding, callback) {
        observer.observeSentChunk(chunk, encoding);
        return ReflectApply(observer.originalWrite, this, arguments);
      };
    }

    this.addCleanupTarget(this.primary);
    this.addCleanupTarget(this.readable);
    this.addCleanupTarget(this.writable);
    this.addCleanupTarget(this.socket);

    if (this.receivedParser !== undefined && head?.byteLength > 0) {
      this.receivedParser.execute(head);
    }
  }

  addCleanupTarget(target) {
    if (target == null ||
        typeof target.once !== 'function' ||
        ArrayPrototypeIncludes(this.cleanupTargets, target)) {
      return;
    }

    this.cleanupTargets.push(target);
    target.once('close', this.cleanupBound);
    target.once('error', this.cleanupBound);
  }

  observeReceivedChunk(chunk, encoding) {
    const buffer = toBuffer(chunk, encoding);
    if (buffer !== null) {
      this.receivedParser.execute(buffer);
    }
  }

  observeSentChunk(chunk, encoding) {
    const buffer = toBuffer(chunk, encoding);
    if (buffer === null) {
      return;
    }

    if (this.sentPreludeSkipper !== undefined) {
      this.sentPreludeSkipper.execute(buffer, (remainder) => this.sentParser.execute(remainder));
      return;
    }

    this.sentParser.execute(buffer);
  }

  cleanup() {
    if (this.closed) {
      return;
    }

    this.closed = true;

    for (let i = 0; i < this.cleanupTargets.length; i++) {
      const target = this.cleanupTargets[i];
      target.removeListener('close', this.cleanupBound);
      target.removeListener('error', this.cleanupBound);
    }

    if (this.originalPush !== null) {
      this.readable.push = this.originalPush;
    }

    if (this.originalWrite !== null) {
      this.writable.write = this.originalWrite;
    }

    this.primary[kObserverState] = undefined;
  }
}

function observeClientWebSocketUpgrade(request, response, socket, head) {
  if (!onClientRequestUpgradeChannel.hasSubscribers &&
      !onClientFrameReceivedChannel.hasSubscribers &&
      !onClientFrameSentChannel.hasSubscribers) {
    return;
  }

  if (!isObservableClientWebSocketUpgrade(request, response)) {
    return;
  }

  if (onClientRequestUpgradeChannel.hasSubscribers) {
    onClientRequestUpgradeChannel.publish({
      request,
      response,
      socket,
      head,
      protocol: 'websocket',
    });
  }

  const observeReceived = onClientFrameReceivedChannel.hasSubscribers;
  const observeSent = onClientFrameSentChannel.hasSubscribers;

  if (!observeReceived && !observeSent) {
    return;
  }

  if (socket[kObserverState] !== undefined) {
    return;
  }

  const observer = new WebSocketObserver({
    request,
    response,
    socket,
    primary: socket,
    readable: socket,
    writable: socket,
    onFrameReceived: observeReceived ?
      onClientFrameReceivedChannel.publish.bind(onClientFrameReceivedChannel) :
      undefined,
    onFrameSent: observeSent ?
      onClientFrameSentChannel.publish.bind(onClientFrameSentChannel) :
      undefined,
  });
  socket[kObserverState] = observer;
  observer.enable(head);
}

function observeServerWebSocketUpgrade(request, socketOrStream, socket, head, parseHead = true) {
  if (!onServerRequestUpgradeChannel.hasSubscribers &&
      !onServerFrameReceivedChannel.hasSubscribers &&
      !onServerFrameSentChannel.hasSubscribers) {
    return;
  }

  if (!hasWebSocketHeader(request.headers?.upgrade)) {
    return;
  }

  if (onServerRequestUpgradeChannel.hasSubscribers) {
    onServerRequestUpgradeChannel.publish({
      request,
      socketOrStream,
      head,
      protocol: 'websocket',
    });
  }

  const observeReceived = onServerFrameReceivedChannel.hasSubscribers;
  const observeSent = onServerFrameSentChannel.hasSubscribers;

  if (!observeReceived && !observeSent) {
    return;
  }

  if (socketOrStream[kObserverState] !== undefined) {
    return;
  }

  const observer = new WebSocketObserver({
    request,
    socket,
    primary: socketOrStream,
    readable: socketOrStream,
    writable: socketOrStream,
    onFrameReceived: observeReceived ?
      onServerFrameReceivedChannel.publish.bind(onServerFrameReceivedChannel) :
      undefined,
    onFrameSent: observeSent ?
      onServerFrameSentChannel.publish.bind(onServerFrameSentChannel) :
      undefined,
    skipHttpUpgradeResponse: observeSent,
  });
  socketOrStream[kObserverState] = observer;
  observer.enable(parseHead ? head : undefined);
}

module.exports = {
  hasWebSocketHeader,
  observeClientWebSocketUpgrade,
  observeServerWebSocketUpgrade,
};
