'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

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
