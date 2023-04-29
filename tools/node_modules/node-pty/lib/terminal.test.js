"use strict";
/**
 * Copyright (c) 2017, Daniel Imms (MIT License).
 * Copyright (c) 2018, Microsoft Corporation (MIT License).
 */
var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
var assert = require("assert");
var windowsTerminal_1 = require("./windowsTerminal");
var unixTerminal_1 = require("./unixTerminal");
var terminal_1 = require("./terminal");
var terminalConstructor = (process.platform === 'win32') ? windowsTerminal_1.WindowsTerminal : unixTerminal_1.UnixTerminal;
var SHELL = (process.platform === 'win32') ? 'cmd.exe' : '/bin/bash';
var terminalCtor;
if (process.platform === 'win32') {
    terminalCtor = require('./windowsTerminal');
}
else {
    terminalCtor = require('./unixTerminal');
}
var TestTerminal = /** @class */ (function (_super) {
    __extends(TestTerminal, _super);
    function TestTerminal() {
        return _super !== null && _super.apply(this, arguments) || this;
    }
    TestTerminal.prototype.checkType = function (name, value, type, allowArray) {
        if (allowArray === void 0) { allowArray = false; }
        this._checkType(name, value, type, allowArray);
    };
    TestTerminal.prototype._write = function (data) {
        throw new Error('Method not implemented.');
    };
    TestTerminal.prototype.resize = function (cols, rows) {
        throw new Error('Method not implemented.');
    };
    TestTerminal.prototype.destroy = function () {
        throw new Error('Method not implemented.');
    };
    TestTerminal.prototype.kill = function (signal) {
        throw new Error('Method not implemented.');
    };
    Object.defineProperty(TestTerminal.prototype, "process", {
        get: function () {
            throw new Error('Method not implemented.');
        },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(TestTerminal.prototype, "master", {
        get: function () {
            throw new Error('Method not implemented.');
        },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(TestTerminal.prototype, "slave", {
        get: function () {
            throw new Error('Method not implemented.');
        },
        enumerable: true,
        configurable: true
    });
    return TestTerminal;
}(terminal_1.Terminal));
describe('Terminal', function () {
    describe('constructor', function () {
        it('should do basic type checks', function () {
            assert.throws(function () { return new terminalCtor('a', 'b', { 'name': {} }); }, 'name must be a string (not a object)');
        });
    });
    describe('checkType', function () {
        it('should throw for the wrong type', function () {
            var t = new TestTerminal();
            assert.doesNotThrow(function () { return t.checkType('foo', 'test', 'string'); });
            assert.doesNotThrow(function () { return t.checkType('foo', 1, 'number'); });
            assert.doesNotThrow(function () { return t.checkType('foo', {}, 'object'); });
            assert.throws(function () { return t.checkType('foo', 'test', 'number'); });
            assert.throws(function () { return t.checkType('foo', 1, 'object'); });
            assert.throws(function () { return t.checkType('foo', {}, 'string'); });
        });
        it('should throw for wrong types within arrays', function () {
            var t = new TestTerminal();
            assert.doesNotThrow(function () { return t.checkType('foo', ['test'], 'string', true); });
            assert.doesNotThrow(function () { return t.checkType('foo', [1], 'number', true); });
            assert.doesNotThrow(function () { return t.checkType('foo', [{}], 'object', true); });
            assert.throws(function () { return t.checkType('foo', ['test'], 'number', true); });
            assert.throws(function () { return t.checkType('foo', [1], 'object', true); });
            assert.throws(function () { return t.checkType('foo', [{}], 'string', true); });
        });
    });
    describe('automatic flow control', function () {
        it('should respect ctor flow control options', function () {
            var pty = new terminalConstructor(SHELL, [], { handleFlowControl: true, flowControlPause: 'abc', flowControlResume: '123' });
            assert.equal(pty.handleFlowControl, true);
            assert.equal(pty._flowControlPause, 'abc');
            assert.equal(pty._flowControlResume, '123');
        });
        // TODO: I don't think this test ever worked due to pollUntil being used incorrectly
        // it('should do flow control automatically', async function(): Promise<void> {
        //   // Flow control doesn't work on Windows
        //   if (process.platform === 'win32') {
        //     return;
        //   }
        //   this.timeout(10000);
        //   const pty = new terminalConstructor(SHELL, [], {handleFlowControl: true, flowControlPause: 'PAUSE', flowControlResume: 'RESUME'});
        //   let read: string = '';
        //   pty.on('data', data => read += data);
        //   pty.on('pause', () => read += 'paused');
        //   pty.on('resume', () => read += 'resumed');
        //   pty.write('1');
        //   pty.write('PAUSE');
        //   pty.write('2');
        //   pty.write('RESUME');
        //   pty.write('3');
        //   await pollUntil(() => {
        //     return stripEscapeSequences(read).endsWith('1pausedresumed23');
        //   }, 100, 10);
        // });
    });
});
function stripEscapeSequences(data) {
    return data.replace(/\u001b\[0K/, '');
}
//# sourceMappingURL=terminal.test.js.map