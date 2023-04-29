"use strict";
/**
 * Copyright (c) 2017, Daniel Imms (MIT License).
 * Copyright (c) 2018, Microsoft Corporation (MIT License).
 */
Object.defineProperty(exports, "__esModule", { value: true });
var unixTerminal_1 = require("./unixTerminal");
var assert = require("assert");
var cp = require("child_process");
var path = require("path");
var testUtils_test_1 = require("./testUtils.test");
var FIXTURES_PATH = path.normalize(path.join(__dirname, '..', 'fixtures', 'utf8-character.txt'));
if (process.platform !== 'win32') {
    describe('UnixTerminal', function () {
        describe('Constructor', function () {
            it('should set a valid pts name', function () {
                var term = new unixTerminal_1.UnixTerminal('/bin/bash', [], {});
                var regExp;
                if (process.platform === 'linux') {
                    // https://linux.die.net/man/4/pts
                    regExp = /^\/dev\/pts\/\d+$/;
                }
                if (process.platform === 'darwin') {
                    // https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man4/pty.4.html
                    regExp = /^\/dev\/tty[p-sP-S][a-z0-9]+$/;
                }
                if (regExp) {
                    assert.ok(regExp.test(term._pty), '"' + term._pty + '" should match ' + regExp.toString());
                }
            });
        });
        describe('PtyForkEncodingOption', function () {
            it('should default to utf8', function (done) {
                var term = new unixTerminal_1.UnixTerminal('/bin/bash', ['-c', "cat \"" + FIXTURES_PATH + "\""]);
                term.on('data', function (data) {
                    assert.equal(typeof data, 'string');
                    assert.equal(data, '\u00E6');
                    done();
                });
            });
            it('should return a Buffer when encoding is null', function (done) {
                var term = new unixTerminal_1.UnixTerminal('/bin/bash', ['-c', "cat \"" + FIXTURES_PATH + "\""], {
                    encoding: null
                });
                term.on('data', function (data) {
                    assert.equal(typeof data, 'object');
                    assert.ok(data instanceof Buffer);
                    assert.equal(0xC3, data[0]);
                    assert.equal(0xA6, data[1]);
                    done();
                });
            });
            it('should support other encodings', function (done) {
                var text = 'test æ!';
                var term = new unixTerminal_1.UnixTerminal(null, ['-c', 'echo "' + text + '"'], {
                    encoding: 'base64'
                });
                var buffer = '';
                term.on('data', function (data) {
                    assert.equal(typeof data, 'string');
                    buffer += data;
                });
                term.on('exit', function () {
                    assert.equal(Buffer.alloc(8, buffer, 'base64').toString().replace('\r', '').replace('\n', ''), text);
                    done();
                });
            });
        });
        describe('open', function () {
            var term;
            afterEach(function () {
                if (term) {
                    term.slave.destroy();
                    term.master.destroy();
                }
            });
            it('should open a pty with access to a master and slave socket', function (done) {
                term = unixTerminal_1.UnixTerminal.open({});
                var slavebuf = '';
                term.slave.on('data', function (data) {
                    slavebuf += data;
                });
                var masterbuf = '';
                term.master.on('data', function (data) {
                    masterbuf += data;
                });
                testUtils_test_1.pollUntil(function () {
                    if (masterbuf === 'slave\r\nmaster\r\n' && slavebuf === 'master\n') {
                        done();
                        return true;
                    }
                    return false;
                }, 200, 10);
                term.slave.write('slave\n');
                term.master.write('master\n');
            });
        });
        describe('signals in parent and child', function () {
            it('SIGINT - custom in parent and child', function (done) {
                // this test is cumbersome - we have to run it in a sub process to
                // see behavior of SIGINT handlers
                var data = "\n        var pty = require('./lib/index');\n        process.on('SIGINT', () => console.log('SIGINT in parent'));\n        var ptyProcess = pty.spawn('node', ['-e', 'process.on(\"SIGINT\", ()=>console.log(\"SIGINT in child\"));setTimeout(() => null, 300);'], {\n          name: 'xterm-color',\n          cols: 80,\n          rows: 30,\n          cwd: process.env.HOME,\n          env: process.env\n        });\n        ptyProcess.on('data', function (data) {\n          console.log(data);\n        });\n        setTimeout(() => null, 500);\n        console.log('ready', ptyProcess.pid);\n        ";
                var buffer = [];
                var p = cp.spawn('node', ['-e', data]);
                var sub = '';
                p.stdout.on('data', function (data) {
                    if (!data.toString().indexOf('ready')) {
                        sub = data.toString().split(' ')[1].slice(0, -1);
                        setTimeout(function () {
                            process.kill(parseInt(sub), 'SIGINT'); // SIGINT to child
                            p.kill('SIGINT'); // SIGINT to parent
                        }, 200);
                    }
                    else {
                        buffer.push(data.toString().replace(/^\s+|\s+$/g, ''));
                    }
                });
                p.on('close', function () {
                    // handlers in parent and child should have been triggered
                    assert.equal(buffer.indexOf('SIGINT in child') !== -1, true);
                    assert.equal(buffer.indexOf('SIGINT in parent') !== -1, true);
                    done();
                });
            });
            it('SIGINT - custom in parent, default in child', function (done) {
                // this tests the original idea of the signal(...) change in pty.cc:
                // to make sure the SIGINT handler of a pty child is reset to default
                // and does not interfere with the handler in the parent
                var data = "\n        var pty = require('./lib/index');\n        process.on('SIGINT', () => console.log('SIGINT in parent'));\n        var ptyProcess = pty.spawn('node', ['-e', 'setTimeout(() => console.log(\"should not be printed\"), 300);'], {\n          name: 'xterm-color',\n          cols: 80,\n          rows: 30,\n          cwd: process.env.HOME,\n          env: process.env\n        });\n        ptyProcess.on('data', function (data) {\n          console.log(data);\n        });\n        setTimeout(() => null, 500);\n        console.log('ready', ptyProcess.pid);\n        ";
                var buffer = [];
                var p = cp.spawn('node', ['-e', data]);
                var sub = '';
                p.stdout.on('data', function (data) {
                    if (!data.toString().indexOf('ready')) {
                        sub = data.toString().split(' ')[1].slice(0, -1);
                        setTimeout(function () {
                            process.kill(parseInt(sub), 'SIGINT'); // SIGINT to child
                            p.kill('SIGINT'); // SIGINT to parent
                        }, 200);
                    }
                    else {
                        buffer.push(data.toString().replace(/^\s+|\s+$/g, ''));
                    }
                });
                p.on('close', function () {
                    // handlers in parent and child should have been triggered
                    assert.equal(buffer.indexOf('should not be printed') !== -1, false);
                    assert.equal(buffer.indexOf('SIGINT in parent') !== -1, true);
                    done();
                });
            });
            it('SIGHUP default (child only)', function (done) {
                var term = new unixTerminal_1.UnixTerminal('node', ['-e', "\n        console.log('ready');\n        setTimeout(()=>console.log('timeout'), 200);"
                ]);
                var buffer = '';
                term.on('data', function (data) {
                    if (data === 'ready\r\n') {
                        term.kill();
                    }
                    else {
                        buffer += data;
                    }
                });
                term.on('exit', function () {
                    // no timeout in buffer
                    assert.equal(buffer, '');
                    done();
                });
            });
            it('SIGUSR1 - custom in parent and child', function (done) {
                var pHandlerCalled = 0;
                var handleSigUsr = function (h) {
                    return function () {
                        pHandlerCalled += 1;
                        process.removeListener('SIGUSR1', h);
                    };
                };
                process.on('SIGUSR1', handleSigUsr(handleSigUsr));
                var term = new unixTerminal_1.UnixTerminal('node', ['-e', "\n        process.on('SIGUSR1', () => {\n          console.log('SIGUSR1 in child');\n        });\n        console.log('ready');\n        setTimeout(()=>null, 200);"
                ]);
                var buffer = '';
                term.on('data', function (data) {
                    if (data === 'ready\r\n') {
                        process.kill(process.pid, 'SIGUSR1');
                        term.kill('SIGUSR1');
                    }
                    else {
                        buffer += data;
                    }
                });
                term.on('exit', function () {
                    // should have called both handlers and only once
                    assert.equal(pHandlerCalled, 1);
                    assert.equal(buffer, 'SIGUSR1 in child\r\n');
                    done();
                });
            });
        });
    });
}
//# sourceMappingURL=unixTerminal.test.js.map