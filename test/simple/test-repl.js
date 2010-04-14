require("../common");
var sys = require("sys"),
  net = require("net"),
  repl = require("repl"),
  message = "Read, Eval, Print Loop",
  unix_socket_path = "/tmp/node-repl-sock",
  prompt_unix = "node via Unix socket> ",
  prompt_tcp = "node via TCP socket> ",
  server_tcp, server_unix, client_tcp, client_unix, timer;
  
debug('repl test');

// function for REPL to run
invoke_me = function (arg) {
  return "invoked " + arg;
};

function send_expect(list) {
  if (list.length > 0) {
    var cur = list.shift();

    cur.client.expect = cur.expect;
    cur.client.list = list;
    if (cur.send.length > 0) {
      cur.client.write(cur.send);
    }
  }
}

function tcp_test() {
  server_tcp = net.createServer(function (socket) {
    assert.strictEqual(server_tcp, socket.server);
    assert.strictEqual(server_tcp.type, 'tcp4');
    
    socket.addListener("end", function () {
      socket.end();
    });

    repl.start(prompt_tcp, socket);
  });

  server_tcp.addListener('listening', function () {
    var read_buffer = "";
    
    client_tcp = net.createConnection(PORT);

    client_tcp.addListener('connect', function () {
      assert.equal(true, client_tcp.readable);
      assert.equal(true, client_tcp.writable);

      send_expect([
          { client: client_tcp, send: "", expect: prompt_tcp },
          { client: client_tcp, send: "invoke_me(333)", expect: ('\'' + "invoked 333" + '\'\n' + prompt_tcp) },
          { client: client_tcp, send: "a += 1", expect: ("12346" + '\n' + prompt_tcp) }
        ]);
    });

    client_tcp.addListener('data', function (data) {
      read_buffer += data.asciiSlice(0, data.length);
      sys.puts("TCP data: " + read_buffer + ", expecting " + client_tcp.expect);
      if (read_buffer.indexOf(prompt_tcp) !== -1) {
        assert.strictEqual(client_tcp.expect, read_buffer);
        read_buffer = "";
        if (client_tcp.list && client_tcp.list.length > 0) {
          send_expect(client_tcp.list);
        }
        else {
          sys.puts("End of TCP test.");
          client_tcp.end();
          client_unix.end();
          clearTimeout(timer);
        }
      }
      else {
        sys.puts("didn't see prompt yet, buffering");
      }
    });
  
    client_tcp.addListener("error", function (e) {
      throw e;
    });
    
    client_tcp.addListener("close", function () {
      server_tcp.close();
    });
  });

  server_tcp.listen(PORT);
}

function unix_test() {
  server_unix = net.createServer(function (socket) {
    assert.strictEqual(server_unix, socket.server);
    assert.strictEqual(server_unix.type, 'unix');

    socket.addListener("end", function () {
      socket.end();
    });

    repl.start(prompt_unix, socket).scope.message = message;
  });

  server_unix.addListener('listening', function () {
    var read_buffer = "";
    
    client_unix = net.createConnection(unix_socket_path);

    client_unix.addListener('connect', function () {
      assert.equal(true, client_unix.readable);
      assert.equal(true, client_unix.writable);

      send_expect([
          { client: client_unix, send: "", expect: prompt_unix },
          { client: client_unix, send: "message", expect: ('\'' + message + '\'\n' + prompt_unix) },
          { client: client_unix, send: "invoke_me(987)", expect: ('\'' + "invoked 987" + '\'\n' + prompt_unix) },
          { client: client_unix, send: "a = 12345", expect: ("12345" + '\n' + prompt_unix) }
        ]);
    });

    client_unix.addListener('data', function (data) {
      read_buffer += data.asciiSlice(0, data.length);
      sys.puts("Unix data: " + read_buffer + ", expecting " + client_unix.expect);
      if (read_buffer.indexOf(prompt_unix) !== -1) {
        assert.strictEqual(client_unix.expect, read_buffer);
        read_buffer = "";
        if (client_unix.list && client_unix.list.length > 0) {
          send_expect(client_unix.list);
        }
        else {
          sys.puts("End of Unix test, running TCP test.");
          tcp_test();
        }
      }
      else {
        sys.puts("didn't see prompt yet, bufering.");
      }
    });
  
    client_unix.addListener("error", function (e) {
      throw e;
    });

    client_unix.addListener("close", function () {
      server_unix.close();
    });
  });

  server_unix.listen(unix_socket_path);
}

unix_test();
timer = setTimeout(function () {
  assert.fail("Timeout");
}, 1000);
