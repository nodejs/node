// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');

common.globalCheck = false;

var net = require('net'),
    repl = require('repl'),
    message = 'Read, Eval, Print Loop',
    unix_socket_path = '/tmp/node-repl-sock',
    prompt_unix = 'node via Unix socket> ',
    prompt_tcp = 'node via TCP socket> ',
    prompt_multiline = '... ',
    server_tcp, server_unix, client_tcp, client_unix, timer;

// absolute path to test/fixtures/a.js
var moduleFilename = require('path').join(common.fixturesDir, 'a');

common.error('repl test');

// function for REPL to run
invoke_me = function(arg) {
  return 'invoked ' + arg;
};

function send_expect(list) {
  if (list.length > 0) {
    var cur = list.shift();

    common.error('sending ' + JSON.stringify(cur.send));

    cur.client.expect = cur.expect;
    cur.client.list = list;
    if (cur.send.length > 0) {
      cur.client.write(cur.send);
    }
  }
}

function clean_up() {
  client_tcp.end();
  client_unix.end();
  clearTimeout(timer);
}

function error_test() {
  // The other stuff is done so reuse unix socket
  var read_buffer = '';
  client_unix.removeAllListeners('data');

  client_unix.addListener('data', function(data) {
    read_buffer += data.toString('ascii', 0, data.length);
    common.error('Unix data: ' + JSON.stringify(read_buffer) + ', expecting ' +
                 (client_unix.expect.exec ?
                  client_unix.expect :
                  JSON.stringify(client_unix.expect)));

    if (read_buffer.indexOf(prompt_unix) !== -1) {
      assert.ok(read_buffer.match(client_unix.expect));
      common.error('match');
      read_buffer = '';
      if (client_unix.list && client_unix.list.length > 0) {
        send_expect(client_unix.list);
      } else {
        common.error('End of Error test, running TCP test.');
        tcp_test();
      }

    } else if (read_buffer === prompt_multiline) {
      // Check that you meant to send a multiline test
      assert.strictEqual(prompt_multiline, client_unix.expect);
      read_buffer = '';
      if (client_unix.list && client_unix.list.length > 0) {
        send_expect(client_unix.list);
      } else {
        common.error('End of Error test, running TCP test.\n');
        tcp_test();
      }

    } else {
      common.error('didn\'t see prompt yet, buffering.');
    }
  });

  send_expect([
    // Uncaught error throws and prints out
    { client: client_unix, send: 'throw new Error(\'test error\');',
      expect: /^Error: test error/ },
    // Common syntax error is treated as multiline command
    { client: client_unix, send: 'function test_func() {',
      expect: prompt_multiline },
    // You can recover with the .break command
    { client: client_unix, send: '.break',
      expect: prompt_unix },
    // Can parse valid JSON
    { client: client_unix, send: 'JSON.parse(\'{"valid": "json"}\');',
      expect: '{ valid: \'json\' }'},
    // invalid input to JSON.parse error is special case of syntax error,
    // should throw
    { client: client_unix, send: 'JSON.parse(\'{invalid: \\\'json\\\'}\');',
      expect: /^SyntaxError: Unexpected token ILLEGAL/ },
    // Named functions can be used:
    { client: client_unix, send: 'function blah() { return 1; }',
      expect: prompt_unix },
    { client: client_unix, send: 'blah()',
      expect: "1\n" + prompt_unix },
    // Multiline object
    { client: client_unix, send: '{ a: ',
      expect: prompt_multiline },
    { client: client_unix, send: '1 }',
      expect: "{ a: 1 }" },
    // Multiline anonymous function with comment
    { client: client_unix, send: '(function () {',
      expect: prompt_multiline },
    { client: client_unix, send: '// blah',
      expect: prompt_multiline },
    { client: client_unix, send: 'return 1;',
      expect: prompt_multiline },
    { client: client_unix, send: '})()',
      expect: "1" },
  ]);
}

function tcp_test() {
  server_tcp = net.createServer(function(socket) {
    assert.strictEqual(server_tcp, socket.server);
    assert.strictEqual(server_tcp.type, 'tcp4');

    socket.addListener('end', function() {
      socket.end();
    });

    repl.start(prompt_tcp, socket);
  });

  server_tcp.listen(common.PORT, function() {
    var read_buffer = '';

    client_tcp = net.createConnection(common.PORT);

    client_tcp.addListener('connect', function() {
      assert.equal(true, client_tcp.readable);
      assert.equal(true, client_tcp.writable);

      send_expect([
        { client: client_tcp, send: '',
          expect: prompt_tcp },
        { client: client_tcp, send: 'invoke_me(333)\n',
          expect: ('\'' + 'invoked 333' + '\'\n' + prompt_tcp) },
        { client: client_tcp, send: 'a += 1\n',
          expect: ('12346' + '\n' + prompt_tcp) },
        { client: client_tcp,
          send: 'require(\'' + moduleFilename + '\').number\n',
          expect: ('42' + '\n' + prompt_tcp) }
      ]);
    });

    client_tcp.addListener('data', function(data) {
      read_buffer += data.toString('ascii', 0, data.length);
      common.error('TCP data: ' + JSON.stringify(read_buffer) +
                   ', expecting ' + JSON.stringify(client_tcp.expect));
      if (read_buffer.indexOf(prompt_tcp) !== -1) {
        assert.strictEqual(client_tcp.expect, read_buffer);
        common.error('match');
        read_buffer = '';
        if (client_tcp.list && client_tcp.list.length > 0) {
          send_expect(client_tcp.list);
        } else {
          common.error('End of TCP test.\n');
          clean_up();
        }
      } else {
        common.error('didn\'t see prompt yet, buffering');
      }
    });

    client_tcp.addListener('error', function(e) {
      throw e;
    });

    client_tcp.addListener('close', function() {
      server_tcp.close();
    });
  });

}

function unix_test() {
  server_unix = net.createServer(function(socket) {
    assert.strictEqual(server_unix, socket.server);
    assert.strictEqual(server_unix.type, 'unix');

    socket.addListener('end', function() {
      socket.end();
    });

    repl.start(prompt_unix, socket).context.message = message;
  });

  server_unix.addListener('listening', function() {
    var read_buffer = '';

    client_unix = net.createConnection(unix_socket_path);

    client_unix.addListener('connect', function() {
      assert.equal(true, client_unix.readable);
      assert.equal(true, client_unix.writable);

      send_expect([
        { client: client_unix, send: '',
          expect: prompt_unix },
        { client: client_unix, send: 'message\n',
          expect: ('\'' + message + '\'\n' + prompt_unix) },
        { client: client_unix, send: 'invoke_me(987)\n',
          expect: ('\'' + 'invoked 987' + '\'\n' + prompt_unix) },
        { client: client_unix, send: 'a = 12345\n',
          expect: ('12345' + '\n' + prompt_unix) },
        { client: client_unix, send: '{a:1}\n',
          expect: ('{ a: 1 }' + '\n' + prompt_unix) },
      ]);
    });

    client_unix.addListener('data', function(data) {
      read_buffer += data.toString('ascii', 0, data.length);
      common.error('Unix data: ' + JSON.stringify(read_buffer) +
                   ', expecting ' + JSON.stringify(client_unix.expect));
      if (read_buffer.indexOf(prompt_unix) !== -1) {
        assert.strictEqual(client_unix.expect, read_buffer);
        common.error('match');
        read_buffer = '';
        if (client_unix.list && client_unix.list.length > 0) {
          send_expect(client_unix.list);
        } else {
          common.error('End of Unix test, running Error test.\n');
          process.nextTick(error_test);
        }
      } else {
        common.error('didn\'t see prompt yet, buffering.');
      }
    });

    client_unix.addListener('error', function(e) {
      throw e;
    });

    client_unix.addListener('close', function() {
      server_unix.close();
    });
  });

  server_unix.listen(unix_socket_path);
}

unix_test();

timer = setTimeout(function() {
  assert.fail('Timeout');
}, 5000);
