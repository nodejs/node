"use strict";
/**
 * Copyright (c) 2017, Daniel Imms (MIT License).
 * Copyright (c) 2018, Microsoft Corporation (MIT License).
 */
Object.defineProperty(exports, "__esModule", { value: true });
var fs = require("fs");
var assert = require("assert");
var windowsTerminal_1 = require("./windowsTerminal");
var path = require("path");
var psList = require("ps-list");
function pollForProcessState(desiredState, intervalMs, timeoutMs) {
    if (intervalMs === void 0) { intervalMs = 100; }
    if (timeoutMs === void 0) { timeoutMs = 2000; }
    return new Promise(function (resolve) {
        var tries = 0;
        var interval = setInterval(function () {
            psList({ all: true }).then(function (ps) {
                var success = true;
                var pids = Object.keys(desiredState).map(function (k) { return parseInt(k, 10); });
                pids.forEach(function (pid) {
                    if (desiredState[pid]) {
                        if (!ps.some(function (p) { return p.pid === pid; })) {
                            success = false;
                        }
                    }
                    else {
                        if (ps.some(function (p) { return p.pid === pid; })) {
                            success = false;
                        }
                    }
                });
                if (success) {
                    clearInterval(interval);
                    resolve();
                    return;
                }
                tries++;
                if (tries * intervalMs >= timeoutMs) {
                    clearInterval(interval);
                    var processListing = pids.map(function (k) { return k + ": " + desiredState[k]; }).join('\n');
                    assert.fail("Bad process state, expected:\n" + processListing);
                    resolve();
                }
            });
        }, intervalMs);
    });
}
function pollForProcessTreeSize(pid, size, intervalMs, timeoutMs) {
    if (intervalMs === void 0) { intervalMs = 100; }
    if (timeoutMs === void 0) { timeoutMs = 2000; }
    return new Promise(function (resolve) {
        var tries = 0;
        var interval = setInterval(function () {
            psList({ all: true }).then(function (ps) {
                var openList = [];
                openList.push(ps.filter(function (p) { return p.pid === pid; }).map(function (p) {
                    return { name: p.name, pid: p.pid };
                })[0]);
                var list = [];
                var _loop_1 = function () {
                    var current = openList.shift();
                    ps.filter(function (p) { return p.ppid === current.pid; }).map(function (p) {
                        return { name: p.name, pid: p.pid };
                    }).forEach(function (p) { return openList.push(p); });
                    list.push(current);
                };
                while (openList.length) {
                    _loop_1();
                }
                var success = list.length === size;
                if (success) {
                    clearInterval(interval);
                    resolve(list);
                    return;
                }
                tries++;
                if (tries * intervalMs >= timeoutMs) {
                    clearInterval(interval);
                    assert.fail("Bad process state, expected: " + size + ", actual: " + list.length);
                    resolve();
                }
            });
        }, intervalMs);
    });
}
if (process.platform === 'win32') {
    describe('WindowsTerminal', function () {
        describe('kill', function () {
            it('should not crash parent process', function (done) {
                var term = new windowsTerminal_1.WindowsTerminal('cmd.exe', [], {});
                term.kill();
                // Add done call to deferred function queue to ensure the kill call has completed
                term._defer(done);
            });
            it('should kill the process tree', function (done) {
                this.timeout(5000);
                var term = new windowsTerminal_1.WindowsTerminal('cmd.exe', [], {});
                // Start sub-processes
                term.write('powershell.exe\r');
                term.write('notepad.exe\r');
                term.write('node.exe\r');
                pollForProcessTreeSize(term.pid, 4, 500, 5000).then(function (list) {
                    assert.equal(list[0].name, 'cmd.exe');
                    assert.equal(list[1].name, 'powershell.exe');
                    assert.equal(list[2].name, 'notepad.exe');
                    assert.equal(list[3].name, 'node.exe');
                    term.kill();
                    var desiredState = {};
                    desiredState[list[0].pid] = false;
                    desiredState[list[1].pid] = false;
                    desiredState[list[2].pid] = true;
                    desiredState[list[3].pid] = false;
                    pollForProcessState(desiredState).then(function () {
                        // Kill notepad before done
                        process.kill(list[2].pid);
                        done();
                    });
                });
            });
        });
        describe('resize', function () {
            it('should throw a non-native exception when resizing an invalid value', function () {
                var term = new windowsTerminal_1.WindowsTerminal('cmd.exe', [], {});
                assert.throws(function () { return term.resize(-1, -1); });
                assert.throws(function () { return term.resize(0, 0); });
                assert.doesNotThrow(function () { return term.resize(1, 1); });
            });
            it('should throw an non-native exception when resizing a killed terminal', function (done) {
                var term = new windowsTerminal_1.WindowsTerminal('cmd.exe', [], {});
                term._defer(function () {
                    term.on('exit', function () {
                        assert.throws(function () { return term.resize(1, 1); });
                        done();
                    });
                    term.destroy();
                });
            });
        });
        describe('Args as CommandLine', function () {
            it('should not fail running a file containing a space in the path', function (done) {
                var spaceFolder = path.resolve(__dirname, '..', 'fixtures', 'space folder');
                if (!fs.existsSync(spaceFolder)) {
                    fs.mkdirSync(spaceFolder);
                }
                var cmdCopiedPath = path.resolve(spaceFolder, 'cmd.exe');
                var data = fs.readFileSync(process.env.windir + "\\System32\\cmd.exe");
                fs.writeFileSync(cmdCopiedPath, data);
                if (!fs.existsSync(cmdCopiedPath)) {
                    // Skip test if git bash isn't installed
                    return;
                }
                var term = new windowsTerminal_1.WindowsTerminal(cmdCopiedPath, '/c echo "hello world"', {});
                var result = '';
                term.on('data', function (data) {
                    result += data;
                });
                term.on('exit', function () {
                    assert.ok(result.indexOf('hello world') >= 1);
                    done();
                });
            });
        });
        describe('env', function () {
            it('should set environment variables of the shell', function (done) {
                var term = new windowsTerminal_1.WindowsTerminal('cmd.exe', '/C echo %FOO%', { env: { FOO: 'BAR' } });
                var result = '';
                term.on('data', function (data) {
                    result += data;
                });
                term.on('exit', function () {
                    assert.ok(result.indexOf('BAR') >= 0);
                    done();
                });
            });
        });
        describe('On close', function () {
            it('should return process zero exit codes', function (done) {
                var term = new windowsTerminal_1.WindowsTerminal('cmd.exe', '/C exit');
                term.on('exit', function (code) {
                    assert.equal(code, 0);
                    done();
                });
            });
            it('should return process non-zero exit codes', function (done) {
                var term = new windowsTerminal_1.WindowsTerminal('cmd.exe', '/C exit 2');
                term.on('exit', function (code) {
                    assert.equal(code, 2);
                    done();
                });
            });
        });
        describe('winpty', function () {
            it('should accept input', function (done) {
                var term = new windowsTerminal_1.WindowsTerminal('cmd.exe', '', { useConpty: false });
                term.write('exit\r');
                term.on('exit', function () {
                    done();
                });
            });
        });
    });
}
//# sourceMappingURL=windowsTerminal.test.js.map