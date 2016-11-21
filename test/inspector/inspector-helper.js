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

function parseWSFrame(buffer, handler) {
  if (buffer.length < 2)
    return 0;
  assert.strictEqual(0x81, buffer[0]);
  let dataLen = 0x7F & buffer[1];
  let bodyOffset = 2;
  if (buffer.length < bodyOffset + dataLen)
    return 0;
  if (dataLen === 126) {
    dataLen = buffer.readUInt16BE(2);
    bodyOffset = 4;
  } else if (dataLen === 127) {
    dataLen = buffer.readUInt32BE(2);
    bodyOffset = 10;
  }
  if (buffer.length < bodyOffset + dataLen)
    return 0;
  const message = JSON.parse(
      buffer.slice(bodyOffset, bodyOffset + dataLen).toString('utf8'));
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

function checkHttpResponse(port, path, callback) {
  http.get({port, path}, function(res) {
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
    for (var line of lines)
      dataCallback(line);
  };
}

function timeout(message, multiplicator) {
  return setTimeout(() => common.fail(message), TIMEOUT * (multiplicator || 1));
}

const TestSession = function(socket, harness) {
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

  let buffer = Buffer.alloc(0);
  socket.on('data', (data) => {
    buffer = Buffer.concat([buffer, data]);
    let consumed;
    do {
      consumed = parseWSFrame(buffer, this.processMessage_.bind(this));
      if (consumed)
        buffer = buffer.slice(consumed);
    } while (consumed);
  }).on('close', () => assert(this.expectClose_, 'Socket closed prematurely'));
};

TestSession.prototype.scriptUrlForId = function(id) {
  return this.scripts_[id];
};

TestSession.prototype.processMessage_ = function(message) {
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
    assert.strictEqual(id, this.expectedId_);
    this.expectedId_++;
    if (this.responseCheckers_[id]) {
      assert(message['result'], JSON.stringify(message) + ' (response to ' +
             JSON.stringify(this.messages_[id]) + ')');
      this.responseCheckers_[id](message['result']);
      delete this.responseCheckers_[id];
    }
    assert(!message['error'], JSON.stringify(message) + ' (replying to ' +
           JSON.stringify(this.messages_[id]) + ')');
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
    this.lastId_++;
    let command = commands[0];
    if (command instanceof Array) {
      this.responseCheckers_[this.lastId_] = command[1];
      command = command[0];
    }
    if (command instanceof Function)
      command = command();
    this.messages_[this.lastId_] = command;
    send(this.socket_, command, this.lastId_,
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
        let s = '';
        for (const id in this.messages_) {
          s += id + ', ';
        }
        common.fail('Messages without response: ' +
                    s.substring(0, s.length - 2));
      }, TIMEOUT);
    });
  });
};

TestSession.prototype.createCallbackWithTimeout_ = function(message) {
  var promise = new Promise((resolve) => {
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
      'Matching response was not received:\n' + expects[0]);
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
      this.createCallbackWithTimeout_('Timed out waiting for ' + regexp));
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
    this.harness_.childInstanceDone =
        this.harness_.childInstanceDone || childDone;
    this.socket_.destroy();
    console.log('[test]', 'Connection terminated');
    callback();
  });
};

TestSession.prototype.testHttpResponse = function(path, check) {
  return this.enqueue((callback) =>
      checkHttpResponse(this.harness_.port, path, (err, response) => {
        check.call(this, err, response);
        callback();
      }));
};


const Harness = function(port, childProcess) {
  this.port = port;
  this.mainScriptPath = mainScript;
  this.stderrFilters_ = [];
  this.process_ = childProcess;
  this.childInstanceDone = false;
  this.returnCode_ = null;
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
    assert(this.childInstanceDone, 'Child instance died prematurely');
    this.returnCode_ = code;
    this.running_ = false;
  });
};

Harness.prototype.addStderrFilter = function(regexp, callback) {
  this.stderrFilters_.push((message) => {
    if (message.match(regexp)) {
      callback();
      return true;
    }
  });
};

Harness.prototype.run_ = function() {
  setImmediate(() => {
    this.task_(() => {
      this.task_ = this.task_.next_;
      if (this.task_)
        this.run_();
    });
  });
};

Harness.prototype.enqueue_ = function(task) {
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

Harness.prototype.testHttpResponse = function(path, check) {
  return this.enqueue_((doneCallback) => {
    checkHttpResponse(this.port, path, (err, response) => {
      check.call(this, err, response);
      doneCallback();
    });
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
  }).on('response', () => common.fail('Upgrade was not received'));
};

Harness.prototype.runFrontendSession = function(tests) {
  return this.enqueue_((callback) => {
    checkHttpResponse(this.port, '/json/list', (err, response) => {
      assert.ifError(err);
      this.wsHandshake(response[0]['webSocketDebuggerUrl'], tests, callback);
    });
  });
};

Harness.prototype.expectShutDown = function(errorCode) {
  this.enqueue_((callback) => {
    if (this.running_) {
      const timeoutId = timeout('Have not terminated');
      this.process_.on('exit', (code) => {
        clearTimeout(timeoutId);
        assert.strictEqual(errorCode, code);
        callback();
      });
    } else {
      assert.strictEqual(errorCode, this.returnCode_);
      callback();
    }
  });
};

exports.startNodeForInspectorTest = function(callback) {
  const child = spawn(process.execPath,
      [ '--inspect', '--debug-brk', mainScript ]);

  const timeoutId = timeout('Child process did not start properly', 4);

  let found = false;

  const dataCallback = makeBufferingDataCallback((text) => {
    clearTimeout(timeoutId);
    console.log('[err]', text);
    if (found) return;
    const match = text.match(/Debugger listening on port (\d+)/);
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
