'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const http = require('http');
const fixtures = require('../common/fixtures');
const { spawn } = require('child_process');
const url = require('url');

const _MAINSCRIPT = fixtures.path('loop.js');
const DEBUG = false;
const TIMEOUT = common.platformTimeout(15 * 1000);

function spawnChildProcess(inspectorFlags, scriptContents, scriptFile) {
  const args = [].concat(inspectorFlags);
  if (scriptContents) {
    args.push('-e', scriptContents);
  } else {
    args.push(scriptFile);
  }
  const child = spawn(process.execPath, args);

  const handler = tearDown.bind(null, child);
  process.on('exit', handler);
  process.on('uncaughtException', handler);
  process.on('unhandledRejection', handler);
  process.on('SIGINT', handler);

  return child;
}

function makeBufferingDataCallback(dataCallback) {
  let buffer = Buffer.alloc(0);
  return (data) => {
    const newData = Buffer.concat([buffer, data]);
    const str = newData.toString('utf8');
    const lines = str.replace(/\r/g, '').split('\n');
    if (str.endsWith('\n'))
      buffer = Buffer.alloc(0);
    else
      buffer = Buffer.from(lines.pop(), 'utf8');
    for (const line of lines)
      dataCallback(line);
  };
}

function tearDown(child, err) {
  child.kill();
  if (err) {
    console.error(err);
    process.exit(1);
  }
}

function parseWSFrame(buffer) {
  // Protocol described in https://tools.ietf.org/html/rfc6455#section-5
  let message = null;
  if (buffer.length < 2)
    return { length: 0, message };
  if (buffer[0] === 0x88 && buffer[1] === 0x00) {
    return { length: 2, message, closed: true };
  }
  assert.strictEqual(0x81, buffer[0]);
  let dataLen = 0x7F & buffer[1];
  let bodyOffset = 2;
  if (buffer.length < bodyOffset + dataLen)
    return 0;
  if (dataLen === 126) {
    dataLen = buffer.readUInt16BE(2);
    bodyOffset = 4;
  } else if (dataLen === 127) {
    assert(buffer[2] === 0 && buffer[3] === 0, 'Inspector message too big');
    dataLen = buffer.readUIntBE(4, 6);
    bodyOffset = 10;
  }
  if (buffer.length < bodyOffset + dataLen)
    return { length: 0, message };
  const jsonPayload =
    buffer.slice(bodyOffset, bodyOffset + dataLen).toString('utf8');
  try {
    message = JSON.parse(jsonPayload);
  } catch (e) {
    console.error(`JSON.parse() failed for: ${jsonPayload}`);
    throw e;
  }
  if (DEBUG)
    console.log('[received]', JSON.stringify(message));
  return { length: bodyOffset + dataLen, message };
}

function formatWSFrame(message) {
  const messageBuf = Buffer.from(JSON.stringify(message));

  const wsHeaderBuf = Buffer.allocUnsafe(16);
  wsHeaderBuf.writeUInt8(0x81, 0);
  let byte2 = 0x80;
  const bodyLen = messageBuf.length;

  let maskOffset = 2;
  if (bodyLen < 126) {
    byte2 = 0x80 + bodyLen;
  } else if (bodyLen < 65536) {
    byte2 = 0xFE;
    wsHeaderBuf.writeUInt16BE(bodyLen, 2);
    maskOffset = 4;
  } else {
    byte2 = 0xFF;
    wsHeaderBuf.writeUInt32BE(bodyLen, 2);
    wsHeaderBuf.writeUInt32BE(0, 6);
    maskOffset = 10;
  }
  wsHeaderBuf.writeUInt8(byte2, 1);
  wsHeaderBuf.writeUInt32BE(0x01020408, maskOffset);

  for (let i = 0; i < messageBuf.length; i++)
    messageBuf[i] = messageBuf[i] ^ (1 << (i % 4));

  return Buffer.concat([wsHeaderBuf.slice(0, maskOffset + 4), messageBuf]);
}

class InspectorSession {
  constructor(socket, instance) {
    this._instance = instance;
    this._socket = socket;
    this._nextId = 1;
    this._commandResponsePromises = new Map();
    this._unprocessedNotifications = [];
    this._notificationCallback = null;
    this._scriptsIdsByUrl = new Map();

    let buffer = Buffer.alloc(0);
    socket.on('data', (data) => {
      buffer = Buffer.concat([buffer, data]);
      do {
        const { length, message, closed } = parseWSFrame(buffer);
        if (!length)
          break;

        if (closed) {
          socket.write(Buffer.from([0x88, 0x00]));  // WS close frame
        }
        buffer = buffer.slice(length);
        if (message)
          this._onMessage(message);
      } while (true);
    });
    this._terminationPromise = new Promise((resolve) => {
      socket.once('close', resolve);
    });
  }

  waitForServerDisconnect() {
    return this._terminationPromise;
  }

  disconnect() {
    this._socket.destroy();
  }

  _onMessage(message) {
    if (message.id) {
      const { resolve, reject } = this._commandResponsePromises.get(message.id);
      this._commandResponsePromises.delete(message.id);
      if (message.result)
        resolve(message.result);
      else
        reject(message.error);
    } else {
      if (message.method === 'Debugger.scriptParsed') {
        const script = message['params'];
        const scriptId = script['scriptId'];
        const url = script['url'];
        this._scriptsIdsByUrl.set(scriptId, url);
        if (url === _MAINSCRIPT)
          this.mainScriptId = scriptId;
      }

      if (this._notificationCallback) {
        // In case callback needs to install another
        const callback = this._notificationCallback;
        this._notificationCallback = null;
        callback(message);
      } else {
        this._unprocessedNotifications.push(message);
      }
    }
  }

  _sendMessage(message) {
    const msg = JSON.parse(JSON.stringify(message)); // Clone!
    msg['id'] = this._nextId++;
    if (DEBUG)
      console.log('[sent]', JSON.stringify(msg));

    const responsePromise = new Promise((resolve, reject) => {
      this._commandResponsePromises.set(msg['id'], { resolve, reject });
    });

    return new Promise(
      (resolve) => this._socket.write(formatWSFrame(msg), resolve))
      .then(() => responsePromise);
  }

  send(commands) {
    if (Array.isArray(commands)) {
      // Multiple commands means the response does not matter. There might even
      // never be a response.
      return Promise
        .all(commands.map((command) => this._sendMessage(command)))
        .then(() => {});
    } else {
      return this._sendMessage(commands);
    }
  }

  waitForNotification(methodOrPredicate, description) {
    const desc = description || methodOrPredicate;
    const message = `Timed out waiting for matching notification (${desc}))`;
    return common.fires(
      this._asyncWaitForNotification(methodOrPredicate), message, TIMEOUT);
  }

  async _asyncWaitForNotification(methodOrPredicate) {
    function matchMethod(notification) {
      return notification.method === methodOrPredicate;
    }
    const predicate =
        typeof methodOrPredicate === 'string' ? matchMethod : methodOrPredicate;
    let notification = null;
    do {
      if (this._unprocessedNotifications.length) {
        notification = this._unprocessedNotifications.shift();
      } else {
        notification = await new Promise(
          (resolve) => this._notificationCallback = resolve);
      }
    } while (!predicate(notification));
    return notification;
  }

  _isBreakOnLineNotification(message, line, url) {
    if ('Debugger.paused' === message['method']) {
      const callFrame = message['params']['callFrames'][0];
      const location = callFrame['location'];
      assert.strictEqual(url, this._scriptsIdsByUrl.get(location['scriptId']));
      assert.strictEqual(line, location['lineNumber']);
      return true;
    }
  }

  waitForBreakOnLine(line, url) {
    return this
      .waitForNotification(
        (notification) =>
          this._isBreakOnLineNotification(notification, line, url),
        `break on ${url}:${line}`);
  }

  _matchesConsoleOutputNotification(notification, type, values) {
    if (!Array.isArray(values))
      values = [ values ];
    if ('Runtime.consoleAPICalled' === notification['method']) {
      const params = notification['params'];
      if (params['type'] === type) {
        let i = 0;
        for (const value of params['args']) {
          if (value['value'] !== values[i++])
            return false;
        }
        return i === values.length;
      }
    }
  }

  waitForConsoleOutput(type, values) {
    const desc = `Console output matching ${JSON.stringify(values)}`;
    return this.waitForNotification(
      (notification) => this._matchesConsoleOutputNotification(notification,
                                                               type, values),
      desc);
  }

  async runToCompletion() {
    console.log('[test]', 'Verify node waits for the frontend to disconnect');
    await this.send({ 'method': 'Debugger.resume' });
    await this.waitForNotification((notification) => {
      return notification.method === 'Runtime.executionContextDestroyed' &&
        notification.params.executionContextId === 1;
    });
    while ((await this._instance.nextStderrString()) !==
              'Waiting for the debugger to disconnect...');
    await this.disconnect();
  }
}

class NodeInstance {
  constructor(inspectorFlags = ['--inspect-brk=0'],
              scriptContents = '',
              scriptFile = _MAINSCRIPT) {
    this._portCallback = null;
    this.portPromise = new Promise((resolve) => this._portCallback = resolve);
    this._process = spawnChildProcess(inspectorFlags, scriptContents,
                                      scriptFile);
    this._running = true;
    this._stderrLineCallback = null;
    this._unprocessedStderrLines = [];

    this._process.stdout.on('data', makeBufferingDataCallback(
      (line) => console.log('[out]', line)));

    this._process.stderr.on('data', makeBufferingDataCallback(
      (message) => this.onStderrLine(message)));

    this._shutdownPromise = new Promise((resolve) => {
      this._process.once('exit', (exitCode, signal) => {
        resolve({ exitCode, signal });
        this._running = false;
      });
    });
  }

  static async startViaSignal(scriptContents) {
    const instance = new NodeInstance(
      [], `${scriptContents}\nprocess._rawDebug('started');`, undefined);
    const msg = 'Timed out waiting for process to start';
    while (await common.fires(instance.nextStderrString(), msg, TIMEOUT) !==
             'started') {}
    process._debugProcess(instance._process.pid);
    return instance;
  }

  onStderrLine(line) {
    console.log('[err]', line);
    if (this._portCallback) {
      const matches = line.match(/Debugger listening on ws:\/\/.+:(\d+)\/.+/);
      if (matches) {
        this._portCallback(matches[1]);
        this._portCallback = null;
      }
    }
    if (this._stderrLineCallback) {
      this._stderrLineCallback(line);
      this._stderrLineCallback = null;
    } else {
      this._unprocessedStderrLines.push(line);
    }
  }

  httpGet(host, path) {
    console.log('[test]', `Testing ${path}`);
    return this.portPromise.then((port) => new Promise((resolve, reject) => {
      const req = http.get({ host, port, path }, (res) => {
        let response = '';
        res.setEncoding('utf8');
        res
          .on('data', (data) => response += data.toString())
          .on('end', () => {
            resolve(response);
          });
      });
      req.on('error', reject);
    })).then((response) => {
      try {
        return JSON.parse(response);
      } catch (e) {
        e.body = response;
        throw e;
      }
    });
  }

  wsHandshake(devtoolsUrl) {
    return this.portPromise.then((port) => new Promise((resolve) => {
      http.get({
        port,
        path: url.parse(devtoolsUrl).path,
        headers: {
          'Connection': 'Upgrade',
          'Upgrade': 'websocket',
          'Sec-WebSocket-Version': 13,
          'Sec-WebSocket-Key': 'key=='
        }
      }).on('upgrade', (message, socket) => {
        resolve(new InspectorSession(socket, this));
      }).on('response', common.mustNotCall('Upgrade was not received'));
    }));
  }

  async connectInspectorSession() {
    console.log('[test]', 'Connecting to a child Node process');
    const response = await this.httpGet(null, '/json/list');
    const url = response[0]['webSocketDebuggerUrl'];
    return await this.wsHandshake(url);
  }

  expectShutdown() {
    return this._shutdownPromise;
  }

  nextStderrString() {
    if (this._unprocessedStderrLines.length)
      return Promise.resolve(this._unprocessedStderrLines.shift());
    return new Promise((resolve) => this._stderrLineCallback = resolve);
  }

  kill() {
    this._process.kill();
  }
}

function readMainScriptSource() {
  return fs.readFileSync(_MAINSCRIPT, 'utf8');
}

module.exports = {
  mainScriptPath: _MAINSCRIPT,
  readMainScriptSource,
  NodeInstance
};
