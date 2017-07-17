'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const net = require('net');
const providers = Object.assign({}, process.binding('async_wrap').Providers);

// Make sure that all Providers are tested.
{
  const hooks = require('async_hooks').createHook({
    init(id, type) {
      if (type === 'NONE')
        throw new Error('received a provider type of NONE');
      delete providers[type];
    },
  }).enable();
  process.on('beforeExit', common.mustCall(() => {
    process.removeAllListeners('uncaughtException');
    hooks.disable();
    delete providers.NONE;  // Should never be used.

    // TODO(jasnell): Test for these
    delete providers.HTTP2SESSION;
    delete providers.HTTP2SESSIONSHUTDOWNWRAP;

    const obj_keys = Object.keys(providers);
    if (obj_keys.length > 0)
      process._rawDebug(obj_keys);
    assert.strictEqual(obj_keys.length, 0);
  }));
}

function testUninitialized(req, ctor_name) {
  assert.strictEqual(typeof req.getAsyncId, 'function');
  assert.strictEqual(req.getAsyncId(), -1);
  assert.strictEqual(req.constructor.name, ctor_name);
}

function testInitialized(req, ctor_name) {
  assert.strictEqual(typeof req.getAsyncId, 'function');
  assert(Number.isSafeInteger(req.getAsyncId()));
  assert(req.getAsyncId() > 0);
  assert.strictEqual(req.constructor.name, ctor_name);
}


{
  const cares = process.binding('cares_wrap');
  const dns = require('dns');

  testUninitialized(new cares.GetAddrInfoReqWrap(), 'GetAddrInfoReqWrap');
  testUninitialized(new cares.GetNameInfoReqWrap(), 'GetNameInfoReqWrap');
  testUninitialized(new cares.QueryReqWrap(), 'QueryReqWrap');

  testInitialized(dns.lookup('www.google.com', () => {}), 'GetAddrInfoReqWrap');
  testInitialized(dns.lookupService('::1', 22, () => {}), 'GetNameInfoReqWrap');
  testInitialized(dns.resolve6('::1', () => {}), 'QueryReqWrap');
  testInitialized(new cares.ChannelWrap(), 'ChannelWrap');
}


{
  const FSEvent = process.binding('fs_event_wrap').FSEvent;
  testInitialized(new FSEvent(), 'FSEvent');
}


{
  const JSStream = process.binding('js_stream').JSStream;
  testInitialized(new JSStream(), 'JSStream');
}


{
  // We don't want to expose getAsyncId for promises but we need to construct
  // one so that the corresponding provider type is removed from the
  // providers list.
  new Promise((res) => res(5));
}


if (common.hasCrypto) {
  const tls = require('tls');
  // SecurePair
  testInitialized(tls.createSecurePair().ssl, 'Connection');
}


if (common.hasCrypto) {
  const crypto = require('crypto');

  // The handle for PBKDF2 and RandomBytes isn't returned by the function call,
  // so need to check it from the callback.

  const mc = common.mustCall(function pb() {
    testInitialized(this, 'PBKDF2');
  });
  crypto.pbkdf2('password', 'salt', 1, 20, 'sha256', mc);

  crypto.randomBytes(1, common.mustCall(function rb() {
    testInitialized(this, 'RandomBytes');
  }));
}


{
  const binding = process.binding('fs');
  const path = require('path');

  const FSReqWrap = binding.FSReqWrap;
  const req = new FSReqWrap();
  req.oncomplete = () => { };

  testUninitialized(req, 'FSReqWrap');
  binding.access(path._makeLong('../'), fs.F_OK, req);
  testInitialized(req, 'FSReqWrap');

  const StatWatcher = binding.StatWatcher;
  testInitialized(new StatWatcher(), 'StatWatcher');
}


{
  const HTTPParser = process.binding('http_parser').HTTPParser;
  testInitialized(new HTTPParser(), 'HTTPParser');
}


{
  const Gzip = require('zlib').Gzip;
  testInitialized(new Gzip()._handle, 'Zlib');
}


{
  const binding = process.binding('pipe_wrap');
  const handle = new binding.Pipe();
  testInitialized(handle, 'Pipe');
  const req = new binding.PipeConnectWrap();
  testUninitialized(req, 'PipeConnectWrap');
  req.address = common.PIPE;
  req.oncomplete = common.mustCall(() => handle.close());
  handle.connect(req, req.address, req.oncomplete);
  testInitialized(req, 'PipeConnectWrap');
}


{
  const Process = process.binding('process_wrap').Process;
  testInitialized(new Process(), 'Process');
}

{
  const Signal = process.binding('signal_wrap').Signal;
  testInitialized(new Signal(), 'Signal');
}


{
  const binding = process.binding('stream_wrap');
  testUninitialized(new binding.WriteWrap(), 'WriteWrap');
}

{
  const stream_wrap = process.binding('stream_wrap');
  const tcp_wrap = process.binding('tcp_wrap');
  const server = net.createServer(common.mustCall((socket) => {
    server.close();
    socket.on('data', (x) => {
      socket.end();
      socket.destroy();
    });
    socket.resume();
  })).listen(0, common.localhostIPv4, common.mustCall(() => {
    const handle = new tcp_wrap.TCP();
    const req = new tcp_wrap.TCPConnectWrap();
    const sreq = new stream_wrap.ShutdownWrap();
    const wreq = new stream_wrap.WriteWrap();
    testInitialized(handle, 'TCP');
    testUninitialized(req, 'TCPConnectWrap');
    testUninitialized(sreq, 'ShutdownWrap');

    sreq.oncomplete = common.mustCall(() => {
      handle.close();
    });

    wreq.handle = handle;
    wreq.oncomplete = common.mustCall(() => {
      handle.shutdown(sreq);
      testInitialized(sreq, 'ShutdownWrap');
    });
    wreq.async = true;

    req.oncomplete = common.mustCall(() => {
      // Use a long string to make sure the write happens asynchronously.
      const err = handle.writeLatin1String(wreq, 'hi'.repeat(100000));
      if (err)
        throw new Error(`write failed: ${process.binding('uv').errname(err)}`);
      testInitialized(wreq, 'WriteWrap');
    });
    req.address = common.localhostIPv4;
    req.port = server.address().port;
    const err = handle.connect(req, req.address, req.port);
    assert.strictEqual(err, 0);
    testInitialized(req, 'TCPConnectWrap');
  }));
}


{
  const TimerWrap = process.binding('timer_wrap').Timer;
  testInitialized(new TimerWrap(), 'Timer');
}


if (common.hasCrypto) {
  const TCP = process.binding('tcp_wrap').TCP;
  const tcp = new TCP();

  const ca = fs.readFileSync(common.fixturesDir + '/test_ca.pem', 'ascii');
  const cert = fs.readFileSync(common.fixturesDir + '/test_cert.pem', 'ascii');
  const key = fs.readFileSync(common.fixturesDir + '/test_key.pem', 'ascii');
  const credentials = require('tls').createSecureContext({ ca, cert, key });

  // TLSWrap is exposed, but needs to be instantiated via tls_wrap.wrap().
  const tls_wrap = process.binding('tls_wrap');
  testInitialized(
    tls_wrap.wrap(tcp._externalStream, credentials.context, true), 'TLSWrap');
}


{
  // Do our best to grab a tty fd.
  const tty_fd = common.getTTYfd();
  if (tty_fd >= 0) {
    const tty_wrap = process.binding('tty_wrap');
    // fd may still be invalid, so guard against it.
    const handle = (() => {
      try {
        return new tty_wrap.TTY(tty_fd, false);
      } catch (e) {
        return null;
      }
    })();
    if (handle !== null)
      testInitialized(handle, 'TTY');
    else
      delete providers.TTYWRAP;
  } else {
    delete providers.TTYWRAP;
  }
}


{
  const binding = process.binding('udp_wrap');
  const handle = new binding.UDP();
  const req = new binding.SendWrap();
  testInitialized(handle, 'UDP');
  testUninitialized(req, 'SendWrap');

  handle.bind('0.0.0.0', common.PORT, undefined);
  req.address = '127.0.0.1';
  req.port = common.PORT;
  req.oncomplete = () => handle.close();
  handle.send(req, [Buffer.alloc(1)], 1, req.port, req.address, true);
  testInitialized(req, 'SendWrap');
}
