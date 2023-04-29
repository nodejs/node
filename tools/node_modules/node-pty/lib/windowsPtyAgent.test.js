"use strict";
/**
 * Copyright (c) 2017, Daniel Imms (MIT License).
 * Copyright (c) 2018, Microsoft Corporation (MIT License).
 */
Object.defineProperty(exports, "__esModule", { value: true });
var assert = require("assert");
var windowsPtyAgent_1 = require("./windowsPtyAgent");
function check(file, args, expected) {
    assert.equal(windowsPtyAgent_1.argsToCommandLine(file, args), expected);
}
if (process.platform === 'win32') {
    describe('argsToCommandLine', function () {
        describe('Plain strings', function () {
            it('doesn\'t quote plain string', function () {
                check('asdf', [], 'asdf');
            });
            it('doesn\'t escape backslashes', function () {
                check('\\asdf\\qwer\\', [], '\\asdf\\qwer\\');
            });
            it('doesn\'t escape multiple backslashes', function () {
                check('asdf\\\\qwer', [], 'asdf\\\\qwer');
            });
            it('adds backslashes before quotes', function () {
                check('"asdf"qwer"', [], '\\"asdf\\"qwer\\"');
            });
            it('escapes backslashes before quotes', function () {
                check('asdf\\"qwer', [], 'asdf\\\\\\"qwer');
            });
        });
        describe('Quoted strings', function () {
            it('quotes string with spaces', function () {
                check('asdf qwer', [], '"asdf qwer"');
            });
            it('quotes empty string', function () {
                check('', [], '""');
            });
            it('quotes string with tabs', function () {
                check('asdf\tqwer', [], '"asdf\tqwer"');
            });
            it('escapes only the last backslash', function () {
                check('\\asdf \\qwer\\', [], '"\\asdf \\qwer\\\\"');
            });
            it('doesn\'t escape multiple backslashes', function () {
                check('asdf \\\\qwer', [], '"asdf \\\\qwer"');
            });
            it('escapes backslashes before quotes', function () {
                check('asdf \\"qwer', [], '"asdf \\\\\\"qwer"');
            });
            it('escapes multiple backslashes at the end', function () {
                check('asdf qwer\\\\', [], '"asdf qwer\\\\\\\\"');
            });
        });
        describe('Multiple arguments', function () {
            it('joins arguments with spaces', function () {
                check('asdf', ['qwer zxcv', '', '"'], 'asdf "qwer zxcv" "" \\"');
            });
            it('array argument all in quotes', function () {
                check('asdf', ['"surounded by quotes"'], 'asdf \\"surounded by quotes\\"');
            });
            it('array argument quotes in the middle', function () {
                check('asdf', ['quotes "in the" middle'], 'asdf "quotes \\"in the\\" middle"');
            });
            it('array argument quotes near start', function () {
                check('asdf', ['"quotes" near start'], 'asdf "\\"quotes\\" near start"');
            });
            it('array argument quotes near end', function () {
                check('asdf', ['quotes "near end"'], 'asdf "quotes \\"near end\\""');
            });
        });
        describe('Args as CommandLine', function () {
            it('should handle empty string', function () {
                check('file', '', 'file');
            });
            it('should not change args', function () {
                check('file', 'foo bar baz', 'file foo bar baz');
                check('file', 'foo \\ba"r \baz', 'file foo \\ba"r \baz');
            });
        });
        describe('Real-world cases', function () {
            it('quotes within quotes', function () {
                check('cmd.exe', ['/c', 'powershell -noexit -command \'Set-location \"C:\\user\"\''], 'cmd.exe /c "powershell -noexit -command \'Set-location \\\"C:\\user\\"\'"');
            });
            it('space within quotes', function () {
                check('cmd.exe', ['/k', '"C:\\Users\\alros\\Desktop\\test script.bat"'], 'cmd.exe /k \\"C:\\Users\\alros\\Desktop\\test script.bat\\"');
            });
        });
    });
}
//# sourceMappingURL=windowsPtyAgent.test.js.map