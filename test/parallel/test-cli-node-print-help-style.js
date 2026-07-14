'use strict';

const common = require('../common');
const assert = require('assert');
const { execFile } = require('child_process');

// eslint-disable-next-line no-control-regex
const ANSI_SGR_REGEX = new RegExp('\x1b\\[[0-9;]*m', 'g');

function stripAnsi(text) {
  return text.replace(ANSI_SGR_REGEX, '');
}

// Test: FORCE_COLOR=1 produces styled output
{
  const env = { ...process.env, FORCE_COLOR: '1' };
  execFile(process.execPath, ['--help'], { env },
           common.mustSucceed((stdout) => {
             assert.ok(stdout.includes('\x1b[1m'), 'bold for headers should be present');
             assert.ok(stdout.includes('\x1b[32m'), 'green for option names should be present');
             assert.ok(stdout.includes('\x1b[35m'), 'magenta for env var names should be present');
             assert.ok(stdout.includes('\x1b[34m'), 'blue for URL should be present');
             assert.ok(stdout.includes('\x1b[4m'), 'underline for URL should be present');
           }));
}

// Test: NO_COLOR=1 produces plain output
{
  const env = { ...process.env, NO_COLOR: '1' };
  execFile(process.execPath, ['--help'], { env },
           common.mustSucceed((stdout) => {
             assert.ok(stripAnsi(stdout) === stdout,
                       'no ANSI escape sequences should be present with NO_COLOR=1');
           }));
}

// Test: piped (non-TTY, no FORCE_COLOR) produces plain output
{
  const env = { ...process.env };
  delete env.FORCE_COLOR;
  delete env.NO_COLOR;
  delete env.NODE_DISABLE_COLORS;
  execFile(process.execPath, ['--help'], { env },
           common.mustSucceed((stdout) => {
             assert.ok(stripAnsi(stdout) === stdout,
                       'no ANSI escape sequences should be present when piped (non-TTY)');
           }));
}

// Test: alignment preservation - stripped styled output matches plain output
{
  const envStyled = { ...process.env, FORCE_COLOR: '1' };
  const envPlain = { ...process.env, NO_COLOR: '1' };
  execFile(process.execPath, ['--help'], { env: envStyled },
           common.mustSucceed((styledStdout) => {
             execFile(process.execPath, ['--help'], { env: envPlain },
                      common.mustSucceed((plainStdout) => {
                        assert.ok(stripAnsi(styledStdout) === plainStdout,
                                  'stripped styled output should match plain output (alignment preservation)');
                      }));
           }));
}
