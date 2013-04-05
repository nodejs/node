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
    prompt_unix = 'node via Unix socket> ',
    prompt_tcp = 'node via TCP socket> ',
    prompt_multiline = '... ',
    prompt_npm = 'npm should be run outside of the ' +
                 'node repl, in your normal shell.\n' +
                 '(Press Control-D to exit.)\n',
    expect_npm = prompt_npm + prompt_unix,
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
      cur.client.write(cur.send + '\n');
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

  client_unix.on('data', function(data) {
    read_buffer += data.toString('ascii', 0, data.length);
    common.error('Unix data: ' + JSON.stringify(read_buffer) + ', expecting ' +
                 (client_unix.expect.exec ?
                  client_unix.expect :
                  JSON.stringify(client_unix.expect)));

    if (read_buffer.indexOf(prompt_unix) !== -1) {
      // if it's an exact match, then don't do the regexp
      if (read_buffer !== client_unix.expect) {
        assert.ok(read_buffer.match(client_unix.expect));
        common.error('match');
      }
      read_buffer = '';
      if (client_unix.list && client_unix.list.length > 0) {
        send_expect(client_unix.list);
      } else {
        common.error('End of Error test, running TCP test.');
        tcp_test();
      }

    } else if (read_buffer.indexOf(prompt_multiline) !== -1) {
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
    // Floating point numbers are not interpreted as REPL commands.
    { client: client_unix, send: '.1234',
      expect: '0.1234' },
    // Floating point expressions are not interpreted as REPL commands
		{ client: client_unix, send: '.1+.1',
      expect: '0.2' },
    // Can parse valid JSON
    { client: client_unix, send: 'JSON.parse(\'{"valid": "json"}\');',
      expect: '{ valid: \'json\' }'},
    // invalid input to JSON.parse error is special case of syntax error,
    // should throw
    { client: client_unix, send: 'JSON.parse(\'{invalid: \\\'json\\\'}\');',
      expect: /^SyntaxError: Unexpected token i/ },
    // end of input to JSON.parse error is special case of syntax error,
    // should throw
    { client: client_unix, send: 'JSON.parse(\'{\');',
      expect: /^SyntaxError: Unexpected end of input/ },
    // invalid RegExps are a special case of syntax error,
    // should throw
    { client: client_unix, send: '/(/;',
      expect: /^SyntaxError: Invalid regular expression\:/ },
    // invalid RegExp modifiers are a special case of syntax error,
    // should throw (GH-4012)
    { client: client_unix, send: 'new RegExp("foo", "wrong modifier");',
      expect: /^SyntaxError: Invalid flags supplied to RegExp constructor/ },
    // strict mode syntax errors should be caught (GH-5178)
    { client: client_unix, send: '(function() { "use strict"; return 0755; })()',
      expect: /^SyntaxError: Octal literals are not allowed in strict mode/ },
    { client: client_unix, send: '(function() { "use strict"; return { p: 1, p: 2 }; })()',
      expect: /^SyntaxError: Duplicate data property in object literal not allowed in strict mode/ },
    { client: client_unix, send: '(function(a, a, b) { "use strict"; return a + b + c; })()',
      expect: /^SyntaxError: Strict mode function may not have duplicate parameter names/ },
    { client: client_unix, send: '(function() { "use strict"; with (this) {} })()',
      expect: /^SyntaxError: Strict mode code may not include a with statement/ },
    { client: client_unix, send: '(function() { "use strict"; var x; delete x; })()',
      expect: /^SyntaxError: Delete of an unqualified identifier in strict mode/ },
    { client: client_unix, send: '(function() { "use strict"; eval = 17; })()',
      expect: /^SyntaxError: Assignment to eval or arguments is not allowed in strict mode/ },
    { client: client_unix, send: '(function() { "use strict"; if (true){ function f() { } } })()',
      expect: /^SyntaxError: In strict mode code, functions can only be declared at top level or immediately within another function/ },
    // Named functions can be used:
    { client: client_unix, send: 'function blah() { return 1; }',
      expect: prompt_unix },
    { client: client_unix, send: 'blah()',
      expect: '1\n' + prompt_unix },
    // Functions should not evaluate twice (#2773)
    { client: client_unix, send: 'var I = [1,2,3,function() {}]; I.pop()',
      expect: '[Function]' },
    // Multiline object
    { client: client_unix, send: '{ a: ',
      expect: prompt_multiline },
    { client: client_unix, send: '1 }',
      expect: '{ a: 1 }' },
    // Multiline anonymous function with comment
    { client: client_unix, send: '(function () {',
      expect: prompt_multiline },
    { client: client_unix, send: '// blah',
      expect: prompt_multiline },
    { client: client_unix, send: 'return 1;',
      expect: prompt_multiline },
    { client: client_unix, send: '})()',
      expect: '1' },
    // npm prompt error message
    { client: client_unix, send: 'npm install foobar',
      expect: expect_npm },
    { client: client_unix, send: '(function () {\n\nreturn 1;\n})()',
      expect: '1' },
    { client: client_unix, send: '{\n\na: 1\n}',
      expect: '{ a: 1 }' },
    { client: client_unix, send: 'url.format("http://google.com")',
      expect: 'http://google.com/' },
    { client: client_unix, send: 'var path = 42; path',
      expect: '42' }
  ]);
}

function tcp_test() {
  server_tcp = net.createServer(function(socket) {
    assert.strictEqual(server_tcp, socket.server);

    socket.on('end', function() {
      socket.end();
    });

    repl.start(prompt_tcp, socket);
  });

  server_tcp.listen(common.PORT, function() {
    var read_buffer = '';

    client_tcp = net.createConnection(common.PORT);

    client_tcp.on('connect', function() {
      assert.equal(true, client_tcp.readable);
      assert.equal(true, client_tcp.writable);

      send_expect([
        { client: client_tcp, send: '',
          expect: prompt_tcp },
        { client: client_tcp, send: 'invoke_me(333)',
          expect: ('\'' + 'invoked 333' + '\'\n' + prompt_tcp) },
        { client: client_tcp, send: 'a += 1',
          expect: ('12346' + '\n' + prompt_tcp) },
        { client: client_tcp,
          send: 'require(' + JSON.stringify(moduleFilename) + ').number',
          expect: ('42' + '\n' + prompt_tcp) }
      ]);
    });

    client_tcp.on('data', function(data) {
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

    client_tcp.on('error', function(e) {
      throw e;
    });

    client_tcp.on('close', function() {
      server_tcp.close();
    });
  });

}

function unix_test() {
  server_unix = net.createServer(function(socket) {
    assert.strictEqual(server_unix, socket.server);

    socket.on('end', function() {
      socket.end();
    });

    repl.start({
      prompt: prompt_unix,
      input: socket,
      output: socket,
      useGlobal: true
    }).context.message = message;
  });

  server_unix.on('listening', function() {
    var read_buffer = '';

    client_unix = net.createConnection(common.PIPE);

    client_unix.on('connect', function() {
      assert.equal(true, client_unix.readable);
      assert.equal(true, client_unix.writable);

      send_expect([
        { client: client_unix, send: '',
          expect: prompt_unix },
        { client: client_unix, send: 'message',
          expect: ('\'' + message + '\'\n' + prompt_unix) },
        { client: client_unix, send: 'invoke_me(987)',
          expect: ('\'' + 'invoked 987' + '\'\n' + prompt_unix) },
        { client: client_unix, send: 'a = 12345',
          expect: ('12345' + '\n' + prompt_unix) },
        { client: client_unix, send: '{a:1}',
          expect: ('{ a: 1 }' + '\n' + prompt_unix) }
      ]);
    });

    client_unix.on('data', function(data) {
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

    client_unix.on('error', function(e) {
      throw e;
    });

    client_unix.on('close', function() {
      server_unix.close();
    });
  });

  server_unix.listen(common.PIPE);
}

unix_test();

timer = setTimeout(function() {
  assert.fail('Timeout');
}, 5000);
