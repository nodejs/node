'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const http = require('http');
const path = require('path');
const spawn = require('child_process').spawn;
const url = require('url');

const DEBUG = false;
const TIMEOUT = 15 * 1000;
const EXPECT_ALIVE_SYMBOL = Symbol('isAlive');
const DONT_EXPECT_RESPONSE_SYMBOL = Symbol('dontExpectResponse');
const mainScript = path.join(common.fixturesDir, 'loop.js');

function send(socket, message, id, callback) {
  const msg = JSON.parse(JSON.stringify(message)); // Clone!
  msg['id'] = id;
  if (DEBUG)
    console.log('[sent]', JSON.stringify(msg));
  const messageBuf = Buffer.from(JSON.stringify(msg));

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
  socket.write(
    Buffer.concat([wsHeaderBuf.slice(0, maskOffset + 4), messageBuf]),
    callback);
}

function sendEnd(socket) {
  socket.write(Buffer.from([0x88, 0x80, 0x2D, 0x0E, 0x1E, 0xFA]));
}

function parseWSFrame(buffer, handler) {
  // Protocol described in https://tools.ietf.org/html/rfc6455#section-5
  if (buffer.length < 2)
    return 0;
  if (buffer[0] === 0x88 && buffer[1] === 0x00) {
    handler(null);
    return 2;
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
    return 0;
  const jsonPayload =
    buffer.slice(bodyOffset, bodyOffset + dataLen).toString('utf8');
  let message;
  try {
    message = JSON.parse(jsonPayload);
  } catch (e) {
    console.error(`JSON.parse() failed for: ${jsonPayload}`);
    throw e;
  }
  if (DEBUG)
    console.log('[received]', JSON.stringify(message));
  handler(message);
  return bodyOffset + dataLen;
}

function tearDown(child, err) {
  child.kill();
  if (err instanceof Error) {
    console.error(err.stack);
    process.exit(1);
  }
}

function checkHttpResponse(host, port, path, callback, errorcb) {
  const req = http.get({host, port, path}, function(res) {
    let response = '';
    res.setEncoding('utf8');
    res
      .on('data', (data) => response += data.toString())
      .on('end', () => {
        let err = null;
        let json = undefined;
        try {
          json = JSON.parse(response);
        } catch (e) {
          err = e;
          err.response = response;
        }
        callback(err, json);
      });
  });
  if (errorcb)
    req.on('error', errorcb);
}

function makeBufferingDataCallback(dataCallback) {
  let buffer = Buffer.alloc(0);
  return (data) => {
    const newData = Buffer.concat([buffer, data]);
    const str = newData.toString('utf8');
    const lines = str.split('\n');
    if (str.endsWith('\n'))
      buffer = Buffer.alloc(0);
    else
      buffer = Buffer.from(lines.pop(), 'utf8');
    for (const line of lines)
      dataCallback(line);
  };
}

function timeout(message, multiplicator) {
  return setTimeout(common.mustNotCall(message),
                    TIMEOUT * (multiplicator || 1));
}

function TestSession(socket, harness) {
  this.mainScriptPath = harness.mainScriptPath;
  this.mainScriptId = null;

  this.harness_ = harness;
  this.socket_ = socket;
  this.expectClose_ = false;
  this.scripts_ = {};
  this.messagefilter_ = null;
  this.responseCheckers_ = {};
  this.lastId_ = 0;
  this.messages_ = {};
  this.expectedId_ = 1;
  this.lastMessageResponseCallback_ = null;
  this.closeCallback_ = null;

  let buffer = Buffer.alloc(0);
  socket.on('data', (data) => {
    buffer = Buffer.concat([buffer, data]);
    let consumed;
    do {
      consumed = parseWSFrame(buffer, this.processMessage_.bind(this));
      if (consumed)
        buffer = buffer.slice(consumed);
    } while (consumed);
  }).on('close', () => {
    assert(this.expectClose_, 'Socket closed prematurely');
    this.closeCallback_ && this.closeCallback_();
  });
}

TestSession.prototype.scriptUrlForId = function(id) {
  return this.scripts_[id];
};

TestSession.prototype.processMessage_ = function(message) {
  if (message === null) {
    sendEnd(this.socket_);
    return;
  }

  const method = message['method'];
  if (method === 'Debugger.scriptParsed') {
    const script = message['params'];
    const scriptId = script['scriptId'];
    const url = script['url'];
    this.scripts_[scriptId] = url;
    if (url === mainScript)
      this.mainScriptId = scriptId;
  }
  this.messagefilter_ && this.messagefilter_(message);
  const id = message['id'];
  if (id) {
    this.expectedId_++;
    if (this.responseCheckers_[id]) {
      const messageJSON = JSON.stringify(message);
      const idJSON = JSON.stringify(this.messages_[id]);
      assert(message['result'], `${messageJSON} (response to ${idJSON})`);
      this.responseCheckers_[id](message['result']);
      delete this.responseCheckers_[id];
    }
    const messageJSON = JSON.stringify(message);
    const idJSON = JSON.stringify(this.messages_[id]);
    assert(!message['error'], `${messageJSON} (replying to ${idJSON})`);
    delete this.messages_[id];
    if (id === this.lastId_) {
      this.lastMessageResponseCallback_ && this.lastMessageResponseCallback_();
      this.lastMessageResponseCallback_ = null;
    }
  }
};

TestSession.prototype.sendAll_ = function(commands, callback) {
  if (!commands.length) {
    callback();
  } else {
    let id = ++this.lastId_;
    let command = commands[0];
    if (command instanceof Array) {
      this.responseCheckers_[id] = command[1];
      command = command[0];
    }
    if (command instanceof Function)
      command = command();
    if (!command[DONT_EXPECT_RESPONSE_SYMBOL]) {
      this.messages_[id] = command;
    } else {
      id += 100000;
      this.lastId_--;
    }
    send(this.socket_, command, id,
         () => this.sendAll_(commands.slice(1), callback));
  }
};

TestSession.prototype.sendInspectorCommands = function(commands) {
  if (!(commands instanceof Array))
    commands = [commands];
  return this.enqueue((callback) => {
    let timeoutId = null;
    this.lastMessageResponseCallback_ = () => {
      timeoutId && clearTimeout(timeoutId);
      callback();
    };
    this.sendAll_(commands, () => {
      timeoutId = setTimeout(() => {
        assert.fail(`Messages without response: ${
          Object.keys(this.messages_).join(', ')}`);
      }, TIMEOUT);
    });
  });
};

TestSession.prototype.sendCommandsAndExpectClose = function(commands) {
  if (!(commands instanceof Array))
    commands = [commands];
  return this.enqueue((callback) => {
    let timeoutId = null;
    let done = false;
    this.expectClose_ = true;
    this.closeCallback_ = function() {
      if (timeoutId)
        clearTimeout(timeoutId);
      done = true;
      callback();
    };
    this.sendAll_(commands, () => {
      if (!done) {
        timeoutId = timeout('Session still open');
      }
    });
  });
};

TestSession.prototype.createCallbackWithTimeout_ = function(message) {
  const promise = new Promise((resolve) => {
    this.enqueue((callback) => {
      const timeoutId = timeout(message);
      resolve(() => {
        clearTimeout(timeoutId);
        callback();
      });
    });
  });
  return () => promise.then((callback) => callback());
};

TestSession.prototype.expectMessages = function(expects) {
  if (!(expects instanceof Array)) expects = [ expects ];

  const callback = this.createCallbackWithTimeout_(
    `Matching response was not received:\n${expects[0]}`);
  this.messagefilter_ = (message) => {
    if (expects[0](message))
      expects.shift();
    if (!expects.length) {
      this.messagefilter_ = null;
      callback();
    }
  };
  return this;
};

TestSession.prototype.expectStderrOutput = function(regexp) {
  this.harness_.addStderrFilter(
    regexp,
    this.createCallbackWithTimeout_(`Timed out waiting for ${regexp}`));
  return this;
};

TestSession.prototype.runNext_ = function() {
  if (this.task_) {
    setImmediate(() => {
      this.task_(() => {
        this.task_ = this.task_.next_;
        this.runNext_();
      });
    });
  }
};

TestSession.prototype.enqueue = function(task) {
  if (!this.task_) {
    this.task_ = task;
    this.runNext_();
  } else {
    let t = this.task_;
    while (t.next_)
      t = t.next_;
    t.next_ = task;
  }
  return this;
};

TestSession.prototype.disconnect = function(childDone) {
  return this.enqueue((callback) => {
    this.expectClose_ = true;
    this.socket_.destroy();
    console.log('[test]', 'Connection terminated');
    callback();
  }, childDone);
};

TestSession.prototype.expectClose = function() {
  return this.enqueue((callback) => {
    this.expectClose_ = true;
    callback();
  });
};

TestSession.prototype.assertClosed = function() {
  return this.enqueue((callback) => {
    assert.strictEqual(this.closed_, true);
    callback();
  });
};

TestSession.prototype.testHttpResponse = function(path, check) {
  return this.enqueue((callback) =>
    checkHttpResponse(null, this.harness_.port, path, (err, response) => {
      check.call(this, err, response);
      callback();
    }));
};


function Harness(port, childProcess) {
  this.port = port;
  this.mainScriptPath = mainScript;
  this.stderrFilters_ = [];
  this.process_ = childProcess;
  this.result_ = {};
  this.running_ = true;

  childProcess.stdout.on('data', makeBufferingDataCallback(
    (line) => console.log('[out]', line)));


  childProcess.stderr.on('data', makeBufferingDataCallback((message) => {
    const pending = [];
    console.log('[err]', message);
    for (const filter of this.stderrFilters_)
      if (!filter(message)) pending.push(filter);
    this.stderrFilters_ = pending;
  }));
  childProcess.on('exit', (code, signal) => {
    this.result_ = {code, signal};
    this.running_ = false;
  });
}

Harness.prototype.addStderrFilter = function(regexp, callback) {
  this.stderrFilters_.push((message) => {
    if (message.match(regexp)) {
      callback();
      return true;
    }
  });
};

Harness.prototype.assertStillAlive = function() {
  assert.strictEqual(this.running_, true,
                     `Child died: ${JSON.stringify(this.result_)}`);
};

Harness.prototype.run_ = function() {
  setImmediate(() => {
    if (!this.task_[EXPECT_ALIVE_SYMBOL])
      this.assertStillAlive();
    this.task_(() => {
      this.task_ = this.task_.next_;
      if (this.task_)
        this.run_();
    });
  });
};

Harness.prototype.enqueue_ = function(task, expectAlive) {
  task[EXPECT_ALIVE_SYMBOL] = !!expectAlive;
  if (!this.task_) {
    this.task_ = task;
    this.run_();
  } else {
    let chain = this.task_;
    while (chain.next_)
      chain = chain.next_;
    chain.next_ = task;
  }
  return this;
};

Harness.prototype.testHttpResponse = function(host, path, check, errorcb) {
  return this.enqueue_((doneCallback) => {
    function wrap(callback) {
      if (callback) {
        return function() {
          callback(...arguments);
          doneCallback();
        };
      }
    }
    checkHttpResponse(host, this.port, path, wrap(check), wrap(errorcb));
  });
};

Harness.prototype.wsHandshake = function(devtoolsUrl, tests, readyCallback) {
  http.get({
    port: this.port,
    path: url.parse(devtoolsUrl).path,
    headers: {
      'Connection': 'Upgrade',
      'Upgrade': 'websocket',
      'Sec-WebSocket-Version': 13,
      'Sec-WebSocket-Key': 'key=='
    }
  }).on('upgrade', (message, socket) => {
    const session = new TestSession(socket, this);
    if (!(tests instanceof Array))
      tests = [tests];
    function enqueue(tests) {
      session.enqueue((sessionCb) => {
        if (tests.length) {
          tests[0](session);
          session.enqueue((cb2) => {
            enqueue(tests.slice(1));
            cb2();
          });
        } else {
          readyCallback();
        }
        sessionCb();
      });
    }
    enqueue(tests);
  }).on('response', common.mustNotCall('Upgrade was not received'));
};

Harness.prototype.runFrontendSession = function(tests) {
  return this.enqueue_((callback) => {
    checkHttpResponse(null, this.port, '/json/list', (err, response) => {
      assert.ifError(err);
      this.wsHandshake(response[0]['webSocketDebuggerUrl'], tests, callback);
    });
  });
};

Harness.prototype.expectShutDown = function(errorCode) {
  this.enqueue_((callback) => {
    if (this.running_) {
      const timeoutId = timeout('Have not terminated');
      this.process_.on('exit', (code, signal) => {
        clearTimeout(timeoutId);
        assert.strictEqual(errorCode, code, JSON.stringify({code, signal}));
        callback();
      });
    } else {
      assert.strictEqual(errorCode, this.result_.code);
      callback();
    }
  }, true);
};

Harness.prototype.kill = function() {
  return this.enqueue_((callback) => {
    this.process_.kill();
    callback();
  });
};

exports.startNodeForInspectorTest = function(callback,
                                             inspectorFlags = ['--inspect-brk'],
                                             scriptContents = '',
                                             scriptFile = mainScript) {
  const args = [].concat(inspectorFlags);
  if (scriptContents) {
    args.push('-e', scriptContents);
  } else {
    args.push(scriptFile);
  }

  const child = spawn(process.execPath, args);

  const timeoutId = timeout('Child process did not start properly', 4);

  let found = false;

  const dataCallback = makeBufferingDataCallback((text) => {
    clearTimeout(timeoutId);
    console.log('[err]', text);
    if (found) return;
    const match = text.match(/Debugger listening on ws:\/\/.+:(\d+)\/.+/);
    found = true;
    child.stderr.removeListener('data', dataCallback);
    assert.ok(match, text);
    callback(new Harness(match[1], child));
  });

  child.stderr.on('data', dataCallback);

  const handler = tearDown.bind(null, child);

  process.on('exit', handler);
  process.on('uncaughtException', handler);
  process.on('SIGINT', handler);
};

exports.mainScriptSource = function() {
  return fs.readFileSync(mainScript, 'utf8');
};

exports.markMessageNoResponse = function(message) {
  message[DONT_EXPECT_RESPONSE_SYMBOL] = true;
};
