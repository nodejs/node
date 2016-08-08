/* eslint-disable max-len, strict */
var common = require('../common');
var assert = require('assert');

common.globalCheck = false;
common.refreshTmpDir();

const net = require('net');
const repl = require('repl');
const message = 'Read, Eval, Print Loop';
const prompt_unix = 'node via Unix socket> ';
const prompt_tcp = 'node via TCP socket> ';
const prompt_multiline = '... ';
const prompt_npm = 'npm should be run outside of the ' +
                   'node repl, in your normal shell.\r\n' +
                   '(Press Control-D to exit.)\r\n';
const expect_npm = prompt_npm + prompt_unix;
var server_tcp, server_unix, client_tcp, client_unix, replServer;


// absolute path to test/fixtures/a.js
var moduleFilename = require('path').join(common.fixturesDir, 'a');

console.error('repl test');

// function for REPL to run
global.invoke_me = function(arg) {
  return 'invoked ' + arg;
};

function send_expect(list) {
  if (list.length > 0) {
    var cur = list.shift();

    console.error('sending ' + JSON.stringify(cur.send));

    cur.client.expect = cur.expect;
    cur.client.list = list;
    if (cur.send.length > 0) {
      cur.client.write(cur.send + '\r\n');
    }
  }
}

function clean_up() {
  client_tcp.end();
  client_unix.end();
}

function strict_mode_error_test() {
  send_expect([
    { client: client_unix, send: 'ref = 1',
      expect: /^ReferenceError:\sref\sis\snot\sdefined\n\s+at\srepl:1:5/ },
  ]);
}

function error_test() {
  // The other stuff is done so reuse unix socket
  var read_buffer = '';
  var run_strict_test = true;
  client_unix.removeAllListeners('data');

  client_unix.on('data', function(data) {
    read_buffer += data.toString('ascii', 0, data.length);
    console.error('Unix data: ' + JSON.stringify(read_buffer) + ', expecting ' +
                 (client_unix.expect.exec ?
                  client_unix.expect :
                  JSON.stringify(client_unix.expect)));

    if (read_buffer.indexOf(prompt_unix) !== -1) {
      // if it's an exact match, then don't do the regexp
      if (read_buffer !== client_unix.expect) {
        var expect = client_unix.expect;
        if (expect === prompt_multiline)
          expect = /[\.]{3} /;
        assert.ok(read_buffer.match(expect));
        console.error('match');
      }
      read_buffer = '';
      if (client_unix.list && client_unix.list.length > 0) {
        send_expect(client_unix.list);
      } else if (run_strict_test) {
        replServer.replMode = repl.REPL_MODE_STRICT;
        run_strict_test = false;
        strict_mode_error_test();
      } else {
        console.error('End of Error test, running TCP test.');
        tcp_test();
      }

    } else if (read_buffer.indexOf(prompt_multiline) !== -1) {
      // Check that you meant to send a multiline test
      assert.strictEqual(prompt_multiline, client_unix.expect);
      read_buffer = '';
      if (client_unix.list && client_unix.list.length > 0) {
        send_expect(client_unix.list);
      } else if (run_strict_test) {
        replServer.replMode = repl.REPL_MODE_STRICT;
        run_strict_test = false;
        strict_mode_error_test();
      } else {
        console.error('End of Error test, running TCP test.\r\n');
        tcp_test();
      }

    } else {
      console.error('didn\'t see prompt yet, buffering.');
    }
  });

  send_expect([
    // Can handle carriage returns
    { client: client_unix, send: '(function(){return "JungMinu";})()',
      expect: "'JungMinu'\r\n" + prompt_unix },
    { client: client_unix, send: 'const JungMinu="\\r\\nMinwooJung";JungMinu',
      expect: 'MinwooJung' },
    // Uncaught error throws and prints out
    { client: client_unix, send: 'throw new Error(\'test error\');',
      expect: /^Error: test error/ },
    // Common syntax error is treated as multiline command
    { client: client_unix, send: 'function test_func() {',
      expect: prompt_multiline },
    // You can recover with the .break command
    { client: client_unix, send: '.break',
      expect: prompt_unix },
    // But passing the same string to eval() should throw
    { client: client_unix, send: 'eval("function test_func() {")',
      expect: /\bSyntaxError: Unexpected end of input/ },
    // Can handle multiline template literals
    { client: client_unix, send: '`io.js',
      expect: prompt_multiline },
    // Special REPL commands still available
    { client: client_unix, send: '.break',
      expect: prompt_unix },
    // Template expressions can cross lines
    { client: client_unix, send: '`io.js ${"1.0"',
      expect: prompt_multiline },
    { client: client_unix, send: '+ ".2"}`',
      expect: `'io.js 1.0.2'\r\n${prompt_unix}` },
    // Dot prefix in multiline commands aren't treated as commands
    { client: client_unix, send: '("a"',
      expect: prompt_multiline },
    { client: client_unix, send: '.charAt(0))',
      expect: `'a'\r\n${prompt_unix}` },
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
      expect: /\bSyntaxError: Unexpected token i/ },
    // end of input to JSON.parse error is special case of syntax error,
    // should throw
    { client: client_unix, send: 'JSON.parse(\'066\');',
      expect: /\bSyntaxError: Unexpected number/ },
    // should throw
    { client: client_unix, send: 'JSON.parse(\'{\');',
      expect: /\bSyntaxError: Unexpected end of JSON input/ },
    // invalid RegExps are a special case of syntax error,
    // should throw
    { client: client_unix, send: '/(/;',
      expect: /\bSyntaxError: Invalid regular expression\:/ },
    // invalid RegExp modifiers are a special case of syntax error,
    // should throw (GH-4012)
    { client: client_unix, send: 'new RegExp("foo", "wrong modifier");',
      expect: /\bSyntaxError: Invalid flags supplied to RegExp constructor/ },
    // strict mode syntax errors should be caught (GH-5178)
    { client: client_unix, send: '(function() { "use strict"; return 0755; })()',
      expect: /\bSyntaxError: Octal literals are not allowed in strict mode/ },
    { client: client_unix, send: '(function(a, a, b) { "use strict"; return a + b + c; })()',
      expect: /\bSyntaxError: Duplicate parameter name not allowed in this context/ },
    { client: client_unix, send: '(function() { "use strict"; with (this) {} })()',
      expect: /\bSyntaxError: Strict mode code may not include a with statement/ },
    { client: client_unix, send: '(function() { "use strict"; var x; delete x; })()',
      expect: /\bSyntaxError: Delete of an unqualified identifier in strict mode/ },
    { client: client_unix, send: '(function() { "use strict"; eval = 17; })()',
      expect: /\bSyntaxError: Unexpected eval or arguments in strict mode/ },
    { client: client_unix, send: '(function() { "use strict"; if (true) function f() { } })()',
      expect: /\bSyntaxError: In strict mode code, functions can only be declared at top level or immediately within another function/ },
    // Named functions can be used:
    { client: client_unix, send: 'function blah() { return 1; }',
      expect: prompt_unix },
    { client: client_unix, send: 'blah()',
      expect: '1\r\n' + prompt_unix },
    // Functions should not evaluate twice (#2773)
    { client: client_unix, send: 'var I = [1,2,3,function() {}]; I.pop()',
      expect: '[Function]' },
    // Multiline object
    { client: client_unix, send: '{ a: ',
      expect: prompt_multiline },
    { client: client_unix, send: '1 }',
      expect: '{ a: 1 }' },
    // Multiline anonymous function with comment
    { client: client_unix, send: '(function() {',
      expect: prompt_multiline },
    { client: client_unix, send: '// blah',
      expect: prompt_multiline },
    { client: client_unix, send: 'return 1;',
      expect: prompt_multiline },
    { client: client_unix, send: '})()',
      expect: '1' },
    // Multiline function call
    { client: client_unix, send: 'function f(){}; f(f(1,',
      expect: prompt_multiline },
    { client: client_unix, send: '2)',
      expect: prompt_multiline },
    { client: client_unix, send: ')',
      expect: 'undefined\r\n' + prompt_unix },
    // npm prompt error message
    { client: client_unix, send: 'npm install foobar',
      expect: expect_npm },
    { client: client_unix, send: '(function() {\r\n\r\nreturn 1;\r\n})()',
      expect: '1' },
    { client: client_unix, send: '{\r\n\r\na: 1\r\n}',
      expect: '{ a: 1 }' },
    { client: client_unix, send: 'url.format("http://google.com")',
      expect: 'http://google.com/' },
    { client: client_unix, send: 'var path = 42; path',
      expect: '42' },
    // this makes sure that we don't print `undefined` when we actually print
    // the error message
    { client: client_unix, send: '.invalid_repl_command',
      expect: 'Invalid REPL keyword\r\n' + prompt_unix },
    // this makes sure that we don't crash when we use an inherited property as
    // a REPL command
    { client: client_unix, send: '.toString',
      expect: 'Invalid REPL keyword\r\n' + prompt_unix },
    // fail when we are not inside a String and a line continuation is used
    { client: client_unix, send: '[] \\',
      expect: /\bSyntaxError: Unexpected token ILLEGAL/ },
    // do not fail when a String is created with line continuation
    { client: client_unix, send: '\'the\\\r\nfourth\\\r\neye\'',
      expect: prompt_multiline + prompt_multiline +
              '\'thefourtheye\'\r\n' + prompt_unix },
    // Don't fail when a partial String is created and line continuation is used
    // with whitespace characters at the end of the string. We are to ignore it.
    // This test is to make sure that we properly remove the whitespace
    // characters at the end of line, unlike the buggy `trimWhitespace` function
    { client: client_unix, send: '  \t    .break  \t  ',
      expect: prompt_unix },
    // multiline strings preserve whitespace characters in them
    { client: client_unix, send: '\'the \\\r\n   fourth\t\t\\\r\n  eye  \'',
      expect: prompt_multiline + prompt_multiline +
              '\'the    fourth\\t\\t  eye  \'\r\n' + prompt_unix },
    // more than one multiline strings also should preserve whitespace chars
    { client: client_unix, send: '\'the \\\r\n   fourth\' +  \'\t\t\\\r\n  eye  \'',
      expect: prompt_multiline + prompt_multiline +
              '\'the    fourth\\t\\t  eye  \'\r\n' + prompt_unix },
    // using REPL commands within a string literal should still work
    { client: client_unix, send: '\'\\\r\n.break',
      expect: prompt_unix },
    // using REPL command "help" within a string literal should still work
    { client: client_unix, send: '\'thefourth\\\r\n.help\r\neye\'',
      expect: /'thefourtheye'/ },
    // empty lines in the REPL should be allowed
    { client: client_unix, send: '\r\n\r\r\n\r\r\n',
      expect: prompt_unix + prompt_unix + prompt_unix },
    // empty lines in the string literals should not affect the string
    { client: client_unix, send: '\'the\\\r\n\\\r\nfourtheye\'\r\n',
      expect: prompt_multiline + prompt_multiline +
              '\'thefourtheye\'\r\n' + prompt_unix },
    // Regression test for https://github.com/nodejs/node/issues/597
    { client: client_unix,
      send: '/(.)(.)(.)(.)(.)(.)(.)(.)(.)/.test(\'123456789\')\r\n',
      expect: `true\r\n${prompt_unix}` },
    // the following test's result depends on the RegEx's match from the above
    { client: client_unix,
      send: 'RegExp.$1\r\nRegExp.$2\r\nRegExp.$3\r\nRegExp.$4\r\nRegExp.$5\r\n' +
            'RegExp.$6\r\nRegExp.$7\r\nRegExp.$8\r\nRegExp.$9\r\n',
      expect: ['\'1\'\r\n', '\'2\'\r\n', '\'3\'\r\n', '\'4\'\r\n', '\'5\'\r\n', '\'6\'\r\n',
               '\'7\'\r\n', '\'8\'\r\n', '\'9\'\r\n'].join(`${prompt_unix}`) },
    // regression tests for https://github.com/nodejs/node/issues/2749
    { client: client_unix, send: 'function x() {\r\nreturn \'\\r\\n\';\r\n }',
      expect: prompt_multiline + prompt_multiline +
              'undefined\r\n' + prompt_unix },
    { client: client_unix, send: 'function x() {\r\nreturn \'\\\\\';\r\n }',
      expect: prompt_multiline + prompt_multiline +
              'undefined\r\n' + prompt_unix },
    // regression tests for https://github.com/nodejs/node/issues/3421
    { client: client_unix, send: 'function x() {\r\n//\'\r\n }',
      expect: prompt_multiline + prompt_multiline +
              'undefined\r\n' + prompt_unix },
    { client: client_unix, send: 'function x() {\r\n//"\r\n }',
      expect: prompt_multiline + prompt_multiline +
              'undefined\r\n' + prompt_unix },
    { client: client_unix, send: 'function x() {//\'\r\n }',
      expect: prompt_multiline + 'undefined\r\n' + prompt_unix },
    { client: client_unix, send: 'function x() {//"\r\n }',
      expect: prompt_multiline + 'undefined\r\n' + prompt_unix },
    { client: client_unix, send: 'function x() {\r\nvar i = "\'";\r\n }',
      expect: prompt_multiline + prompt_multiline +
              'undefined\r\n' + prompt_unix },
    { client: client_unix, send: 'function x(/*optional*/) {}',
      expect: 'undefined\r\n' + prompt_unix },
    { client: client_unix, send: 'function x(/* // 5 */) {}',
      expect: 'undefined\r\n' + prompt_unix },
    { client: client_unix, send: '// /* 5 */',
      expect: 'undefined\r\n' + prompt_unix },
    { client: client_unix, send: '"//"',
      expect: '\'//\'\r\n' + prompt_unix },
    { client: client_unix, send: '"data /*with*/ comment"',
      expect: '\'data /*with*/ comment\'\r\n' + prompt_unix },
    { client: client_unix, send: 'function x(/*fn\'s optional params*/) {}',
      expect: 'undefined\r\n' + prompt_unix },
    { client: client_unix, send: '/* \'\r\n"\r\n\'"\'\r\n*/',
      expect: 'undefined\r\n' + prompt_unix },
    // REPL should get a normal require() function, not one that allows
    // access to internal modules without the --expose_internals flag.
    { client: client_unix, send: 'require("internal/repl")',
      expect: /^Error: Cannot find module 'internal\/repl'/ },
    // REPL should handle quotes within regexp literal in multiline mode
    { client: client_unix, send: "function x(s) {\r\nreturn s.replace(/'/,'');\r\n}",
      expect: prompt_multiline + prompt_multiline +
            'undefined\r\n' + prompt_unix },
    { client: client_unix, send: "function x(s) {\r\nreturn s.replace(/\'/,'');\r\n}",
      expect: prompt_multiline + prompt_multiline +
            'undefined\r\n' + prompt_unix },
    { client: client_unix, send: 'function x(s) {\r\nreturn s.replace(/"/,"");\r\n}',
      expect: prompt_multiline + prompt_multiline +
            'undefined\r\n' + prompt_unix },
    { client: client_unix, send: 'function x(s) {\r\nreturn s.replace(/.*/,"");\r\n}',
      expect: prompt_multiline + prompt_multiline +
            'undefined\r\n' + prompt_unix },
    { client: client_unix, send: '{ var x = 4; }',
      expect: 'undefined\r\n' + prompt_unix },
    // Illegal token is not recoverable outside string literal, RegExp literal,
    // or block comment. https://github.com/nodejs/node/issues/3611
    { client: client_unix, send: 'a = 3.5e',
      expect: /\bSyntaxError: Unexpected token ILLEGAL/ },
    // Mitigate https://github.com/nodejs/node/issues/548
    { client: client_unix, send: 'function name(){ return "node"; };name()',
      expect: "'node'\r\n" + prompt_unix },
    { client: client_unix, send: 'function name(){ return "nodejs"; };name()',
      expect: "'nodejs'\r\n" + prompt_unix },
    // Avoid emitting repl:line-number for SyntaxError
    { client: client_unix, send: 'a = 3.5e',
      expect: /^(?!repl)/ },
    // Avoid emitting stack trace
    { client: client_unix, send: 'a = 3.5e',
      expect: /^(?!\s+at\s)/gm },
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

  server_tcp.listen(0, function() {
    var read_buffer = '';

    client_tcp = net.createConnection(this.address().port);

    client_tcp.on('connect', function() {
      assert.equal(true, client_tcp.readable);
      assert.equal(true, client_tcp.writable);

      send_expect([
        { client: client_tcp, send: '',
          expect: prompt_tcp },
        { client: client_tcp, send: 'invoke_me(333)',
          expect: ('\'' + 'invoked 333' + '\'\r\n' + prompt_tcp) },
        { client: client_tcp, send: 'a += 1',
          expect: ('12346' + '\r\n' + prompt_tcp) },
        { client: client_tcp,
          send: 'require(' + JSON.stringify(moduleFilename) + ').number',
          expect: ('42' + '\r\n' + prompt_tcp) }
      ]);
    });

    client_tcp.on('data', function(data) {
      read_buffer += data.toString('ascii', 0, data.length);
      console.error('TCP data: ' + JSON.stringify(read_buffer) +
                   ', expecting ' + JSON.stringify(client_tcp.expect));
      if (read_buffer.indexOf(prompt_tcp) !== -1) {
        assert.strictEqual(client_tcp.expect, read_buffer);
        console.error('match');
        read_buffer = '';
        if (client_tcp.list && client_tcp.list.length > 0) {
          send_expect(client_tcp.list);
        } else {
          console.error('End of TCP test.\r\n');
          clean_up();
        }
      } else {
        console.error('didn\'t see prompt yet, buffering');
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

    replServer = repl.start({
      prompt: prompt_unix,
      input: socket,
      output: socket,
      useGlobal: true
    });
    replServer.context.message = message;
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
          expect: ('\'' + message + '\'\r\n' + prompt_unix) },
        { client: client_unix, send: 'invoke_me(987)',
          expect: ('\'' + 'invoked 987' + '\'\r\n' + prompt_unix) },
        { client: client_unix, send: 'a = 12345',
          expect: ('12345' + '\r\n' + prompt_unix) },
        { client: client_unix, send: '{a:1}',
          expect: ('{ a: 1 }' + '\r\n' + prompt_unix) }
      ]);
    });

    client_unix.on('data', function(data) {
      read_buffer += data.toString('ascii', 0, data.length);
      console.error('Unix data: ' + JSON.stringify(read_buffer) +
                   ', expecting ' + JSON.stringify(client_unix.expect));
      if (read_buffer.indexOf(prompt_unix) !== -1) {
        assert.strictEqual(client_unix.expect, read_buffer);
        console.error('match');
        read_buffer = '';
        if (client_unix.list && client_unix.list.length > 0) {
          send_expect(client_unix.list);
        } else {
          console.error('End of Unix test, running Error test.\r\n');
          process.nextTick(error_test);
        }
      } else {
        console.error('didn\'t see prompt yet, buffering.');
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
