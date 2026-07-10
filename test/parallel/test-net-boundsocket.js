'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const net = require('net');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const isLinux = process.platform === 'linux';

// Constructing a BoundSocket binds synchronously and address() reports the
// resolved address, including the OS-assigned ephemeral port when port is 0.
{
  const bound = new net.BoundSocket({ host: '127.0.0.1', port: 0 });
  const addr = bound.address();

  assert.strictEqual(addr.address, '127.0.0.1');
  assert.strictEqual(addr.family, 'IPv4');
  assert.strictEqual(typeof addr.port, 'number');
  assert.ok(addr.port > 0);

  // fd() exposes the underlying descriptor (a real fd on POSIX, -1 on Windows).
  const fd = bound.fd();
  assert.strictEqual(typeof fd, 'number');
  if (!common.isWindows) {
    assert.ok(fd >= 0);
  }

  bound.close();
}

// Defaults the host to the IPv4 wildcard when omitted.
{
  const bound = new net.BoundSocket({ port: 0 });
  const addr = bound.address();
  assert.strictEqual(addr.address, '0.0.0.0');
  assert.strictEqual(addr.family, 'IPv4');
  assert.ok(addr.port > 0);
  bound.close();
}

// No arguments reserves an OS-assigned ephemeral port on the IPv4 wildcard.
{
  const bound = new net.BoundSocket();
  const addr = bound.address();
  assert.strictEqual(addr.address, '0.0.0.0');
  assert.strictEqual(addr.family, 'IPv4');
  assert.ok(addr.port > 0);
  bound.close();
}

// Binding to a port held by a live listener throws EADDRINUSE synchronously.
// libuv defers this error from uv_tcp_bind(), so the constructor forces a
// getsockname() to surface it eagerly. (Two role-neutral, not-yet-listening
// binds to the same port instead coexist, since libuv sets SO_REUSEADDR.)
{
  const server = net.createServer();
  server.listen(0, '127.0.0.1', common.mustCall(() => {
    const { port } = server.address();
    assert.throws(() => {
      new net.BoundSocket({ host: '127.0.0.1', port });
    }, {
      code: 'EADDRINUSE',
      syscall: 'bind',
    });
    server.close();
  }));
}

// Throws synchronously on a non-numeric host (no DNS resolution).
{
  assert.throws(() => new net.BoundSocket({ host: 'localhost', port: 0 }), {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
  });
}

// Rejects a non-string host and a non-object options argument.
{
  assert.throws(() => new net.BoundSocket({ host: 1234 }),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => new net.BoundSocket(0), { code: 'ERR_INVALID_ARG_TYPE' });
}

// Throws synchronously on a non-local address (EADDRNOTAVAIL). 192.0.2.0/24 is
// TEST-NET-1 (RFC 5737) and is never assigned to a local interface.
{
  assert.throws(() => new net.BoundSocket({ host: '192.0.2.1', port: 0 }), {
    code: 'EADDRNOTAVAIL',
    syscall: 'bind',
  });
}

// Binding a privileged port without privilege throws EACCES synchronously.
if (!common.isWindows && process.getuid() !== 0) {
  assert.throws(() => new net.BoundSocket({ host: '127.0.0.1', port: 1 }), {
    code: 'EACCES',
    syscall: 'bind',
  });
}

// An un-adopted handle releases its socket cleanly on close(): the port becomes
// immediately re-bindable.
{
  const bound = new net.BoundSocket({ host: '127.0.0.1', port: 0 });
  const { port } = bound.address();
  bound.close();
  const again = new net.BoundSocket({ host: '127.0.0.1', port });
  assert.strictEqual(again.address().port, port);
  again.close();
}

// Server adoption: server.listen(boundSocket), then a client round-trips.
{
  const bound = new net.BoundSocket({ host: '127.0.0.1', port: 0 });
  const { port } = bound.address();

  const server = net.createServer(common.mustCall((socket) => {
    socket.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'ping');
      socket.end('pong');
    }));
  }));

  server.listen(bound, common.mustCall(() => {
    assert.strictEqual(server.address().port, port);

    // The bound socket has been adopted: address()/fd()/close() now throw.
    assert.throws(() => bound.address(), { code: 'ERR_SOCKET_HANDLE_ADOPTED' });
    assert.throws(() => bound.fd(), { code: 'ERR_SOCKET_HANDLE_ADOPTED' });
    assert.throws(() => bound.close(), { code: 'ERR_SOCKET_HANDLE_ADOPTED' });

    const client = net.connect({ host: '127.0.0.1', port }, () => {
      client.end('ping');
    });
    client.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'pong');
    }));
    client.on('close', common.mustCall(() => server.close()));
  }));
}

// Client adoption: new net.Socket({ handle: boundSocket }).connect(...) honors
// the bound source port and round-trips.
{
  const server = net.createServer(common.mustCall((socket) => {
    socket.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'ping');
      socket.end('pong');
    }));
  }));

  server.listen(0, '127.0.0.1', common.mustCall(() => {
    const serverPort = server.address().port;

    const bound = new net.BoundSocket({ host: '127.0.0.1', port: 0 });
    const localPort = bound.address().port;

    const client = new net.Socket({ handle: bound });

    // Adoption consumed the bound socket.
    assert.throws(() => bound.address(), { code: 'ERR_SOCKET_HANDLE_ADOPTED' });

    client.connect({ host: '127.0.0.1', port: serverPort }, common.mustCall(() => {
      assert.strictEqual(client.localPort, localPort);
      client.end('ping');
    }));
    client.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'pong');
    }));
    client.on('close', common.mustCall(() => server.close()));
  }));
}

// connect() rejects localAddress/localPort when adopting a bound socket: the
// handle already owns the local endpoint.
{
  const bound = new net.BoundSocket({ host: '127.0.0.1', port: 0 });
  const client = new net.Socket({ handle: bound });
  assert.throws(() => {
    client.connect({ host: '127.0.0.1', port: 1, localPort: 0 });
  }, { code: 'ERR_INVALID_ARG_VALUE' });
  client.destroy();
}

// Adopted BoundSocket: connect(2) is synchronous, so localAddress/localPort
// resolve to the concrete source in-tick, before the 'connect' event.
{
  const server = net.createServer();
  server.listen(0, '127.0.0.1', common.mustCall(() => {
    const serverPort = server.address().port;
    const bound = new net.BoundSocket({ host: '127.0.0.1', port: 0 });
    const localPort = bound.address().port;
    const client = new net.Socket({ handle: bound });

    client.connect({ host: '127.0.0.1', port: serverPort });

    assert.strictEqual(client.localAddress, '127.0.0.1');
    assert.strictEqual(client.localPort, localPort);

    client.on('connect', common.mustCall(() => {
      assert.strictEqual(client.localAddress, '127.0.0.1');
      assert.strictEqual(client.localPort, localPort);
      client.destroy();
      server.close();
    }));
  }));
}

// A connect failure is reported via a deferred 'error' event, never thrown
// synchronously. An IPv4-bound handle connecting to an IPv6 literal fails on
// address family mismatch.
{
  const bound = new net.BoundSocket({ host: '127.0.0.1', port: 0 });
  const client = new net.Socket({ handle: bound });
  client.connect({ host: '::1', port: 1 });
  client.on('error', common.mustCall((err) => {
    assert.strictEqual(err.syscall, 'connect');
  }));
}

// reusePort: SO_REUSEPORT permits multiple listeners on the same port. Support
// is platform-dependent, so probe first.
{
  let first;
  try {
    first = new net.BoundSocket({ host: '127.0.0.1', port: 0, reusePort: true });
  } catch {
    first = null; // SO_REUSEPORT unsupported on this platform.
  }
  if (first) {
    const { port } = first.address();
    const second = new net.BoundSocket({ host: '127.0.0.1', port, reusePort: true });

    const s1 = net.createServer();
    const s2 = net.createServer();
    s1.listen(first, common.mustCall(() => {
      s2.listen(second, common.mustCall(() => {
        assert.strictEqual(s1.address().port, port);
        assert.strictEqual(s2.address().port, port);
        s1.close();
        s2.close();
      }));
    }));
  }
}

// The isPipe getter's presence on the prototype is the capability signal that
// this build honors { path }; the value discriminates pipe from TCP binds.
assert.ok('isPipe' in net.BoundSocket.prototype);
{
  const tcpBound = new net.BoundSocket({ port: 0 });
  assert.strictEqual(tcpBound.isPipe, false);
  tcpBound.close();
}

// The path option is mutually exclusive with the TCP options.
for (const extra of [{ host: '127.0.0.1' }, { port: 0 }, { ipv6Only: true },
                     { reusePort: true }]) {
  assert.throws(
    () => new net.BoundSocket({ path: common.PIPE, ...extra }),
    { code: 'ERR_INVALID_ARG_VALUE' });
}

// A non-string path is rejected.
assert.throws(() => new net.BoundSocket({ path: 1234 }),
              { code: 'ERR_INVALID_ARG_TYPE' });

// Pipe bind is synchronous: address() returns the bound path, and a second
// bind to the same path throws EADDRINUSE in the constructor.
{
  const path = `${common.PIPE}-addr`;
  const bound = new net.BoundSocket({ path });
  assert.strictEqual(bound.isPipe, true);
  assert.strictEqual(bound.address(), path);
  assert.throws(() => new net.BoundSocket({ path }),
                { code: 'EADDRINUSE', syscall: 'bind' });
  bound.close();
}

// Server adoption: server.listen(boundSocket) over a path, client
// net.connect({ path }) round-trips, and the connection is a pipe.
{
  const path = `${common.PIPE}-server`;
  const bound = new net.BoundSocket({ path });

  const server = net.createServer(common.mustCall((socket) => {
    assert.strictEqual(socket.remoteFamily, undefined);
    socket.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'ping');
      socket.end('pong');
    }));
  }));

  server.listen(bound, common.mustCall(() => {
    assert.strictEqual(server.address(), path);
    assert.throws(() => bound.address(), { code: 'ERR_SOCKET_HANDLE_ADOPTED' });

    const client = net.connect({ path }, () => {
      client.end('ping');
    });
    client.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'pong');
    }));
    client.on('close', common.mustCall(() => server.close()));
  }));
}

// Client adoption of a bound source pipe: after connect(), localAddress
// reflects the bound source path. Binding a client to a source path is a
// POSIX unix-domain concept.
if (!common.isWindows) {
  const srcPath = `${common.PIPE}-src`;
  const dstPath = `${common.PIPE}-dst`;

  const server = net.createServer(common.mustCall((socket) => {
    socket.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'ping');
      socket.end('pong');
    }));
  }));

  const dst = new net.BoundSocket({ path: dstPath });
  server.listen(dst, common.mustCall(() => {
    const src = new net.BoundSocket({ path: srcPath });
    assert.strictEqual(src.address(), srcPath);

    const client = new net.Socket({ handle: src });
    client.connect({ path: dstPath });

    assert.strictEqual(client.localAddress, srcPath);

    client.on('connect', common.mustCall(() => {
      assert.strictEqual(client.localAddress, srcPath);
      client.end('ping');
    }));
    client.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'pong');
    }));
    client.on('close', common.mustCall(() => server.close()));
  }));
}

// Reconnecting an adopted pipe BoundSocket after destroy: the adopted handle is
// gone, so pipe-ness must come from the path option rather than the (now null)
// handle, otherwise connect() would wrongly attempt a TCP connect.
if (!common.isWindows) {
  const path = `${common.PIPE}-reconnect`;

  const server = net.createServer(common.mustCall((socket) => {
    socket.on('data', (data) => socket.end(data));
  }, 2));

  const bound = new net.BoundSocket({ path: `${path}-src` });
  server.listen(path, common.mustCall(() => {
    const client = new net.Socket({ handle: bound });
    client.connect({ path });
    client.once('connect', common.mustCall(() => {
      client.end('ping');
      client.once('data', common.mustCall((data) => {
        assert.strictEqual(data.toString(), 'ping');
      }));
      client.once('close', common.mustCall(() => {
        // The adopted handle is gone; reconnect must still be a pipe.
        client.connect({ path });
        client.once('connect', common.mustCall(() => {
          client.end('pong');
          client.once('data', common.mustCall((data) => {
            assert.strictEqual(data.toString(), 'pong');
          }));
          client.once('close', common.mustCall(() => server.close()));
        }));
      }));
    }));
  }));
}

// Linux abstract namespace: a leading '\0' binds without creating a filesystem
// entry, and still listens/connects.
if (isLinux) {
  const abstractPath = `\0node-test-boundsocket.${process.pid}`;
  const bound = new net.BoundSocket({ path: abstractPath });
  assert.strictEqual(bound.address(), abstractPath);

  const server = net.createServer(common.mustCall((socket) => {
    socket.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'ping');
      socket.end('pong');
    }));
  }));

  server.listen(bound, common.mustCall(() => {
    // No filesystem entry is created for an abstract socket.
    assert.ok(!fs.existsSync(abstractPath.slice(1)));

    const client = net.connect({ path: abstractPath }, () => {
      client.end('ping');
    });
    client.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'pong');
    }));
    client.on('close', common.mustCall(() => server.close()));
  }));
}

// An abstract path on a non-Linux platform is rejected synchronously.
if (!isLinux) {
  assert.throws(() => new net.BoundSocket({ path: '\0abstract' }),
                { code: 'ERR_INVALID_ARG_VALUE' });
}

// Filesystem bind errors surface synchronously in the constructor. A missing
// parent directory yields EACCES (libuv maps the kernel's ENOENT to EACCES for
// cross-platform parity), and an over-long path yields EINVAL (uv_pipe_bind is
// called with UV_PIPE_NO_TRUNCATE).
if (!common.isWindows) {
  assert.throws(
    () => new net.BoundSocket({ path: `${common.PIPE}-nope/child.sock` }),
    { code: 'EACCES', syscall: 'bind' });

  assert.throws(
    () => new net.BoundSocket({ path: `${common.PIPE}-${'x'.repeat(200)}` }),
    { code: 'EINVAL', syscall: 'bind' });
}

// IPv6 binds: loopback, and ipv6Only dual-stack control.
if (common.hasIPv6) {
  {
    const bound = new net.BoundSocket({ host: '::1', port: 0 });
    const addr = bound.address();
    assert.strictEqual(addr.address, '::1');
    assert.strictEqual(addr.family, 'IPv6');
    assert.ok(addr.port > 0);
    bound.close();
  }

  {
    const bound = new net.BoundSocket({ ipv6Only: true, port: 0 });
    const addr = bound.address();
    assert.strictEqual(addr.address, '::');
    assert.strictEqual(addr.family, 'IPv6');
    bound.close();
  }

  // ipv6Only: false (default) binds the IPv6 wildcard as dual-stack.
  {
    const bound = new net.BoundSocket({ host: '::', port: 0 });
    assert.strictEqual(bound.address().family, 'IPv6');
    bound.close();
  }
}
