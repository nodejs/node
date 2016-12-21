'use strict';
const spawn = require('child_process').spawn;

const CLI =
  process.env.USE_EMBEDDED_NODE_INSPECT === '1' ?
  'inspect' :
  require.resolve('../../cli.js');

function startCLI(args) {
  const child = spawn(process.execPath, [CLI, ...args]);
  let isFirstStdoutChunk = true;

  const outputBuffer = [];
  function bufferOutput(chunk) {
    if (isFirstStdoutChunk) {
      isFirstStdoutChunk = false;
      outputBuffer.push(chunk.replace(/^debug>\s*/, ''));
    } else {
      outputBuffer.push(chunk);
    }
  }

  function getOutput() {
    return outputBuffer.join('').toString()
      .replace(/^[^\n]*?[\b]/mg, '');
  }

  child.stdout.setEncoding('utf8');
  child.stdout.on('data', bufferOutput);
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', bufferOutput);

  if (process.env.VERBOSE === '1') {
    child.stdout.pipe(process.stderr);
    child.stderr.pipe(process.stderr);
  }

  return {
    flushOutput() {
      const output = this.output;
      outputBuffer.length = 0;
      return output;
    },

    waitFor(pattern, timeout = 2000) {
      function checkPattern(str) {
        if (Array.isArray(pattern)) {
          return pattern.every((p) => p.test(str));
        }
        return pattern.test(str);
      }

      return new Promise((resolve, reject) => {
        function checkOutput() {
          if (checkPattern(getOutput())) {
            tearDown(); // eslint-disable-line no-use-before-define
            resolve();
          }
        }

        function onChildExit() {
          tearDown(); // eslint-disable-line no-use-before-define
          reject(new Error(
            `Child quit while waiting for ${pattern}; found: ${this.output}`));
        }

        const timer = setTimeout(() => {
          tearDown(); // eslint-disable-line no-use-before-define
          reject(new Error([
            `Timeout (${timeout}) while waiting for ${pattern}`,
            `found: ${this.output}`,
          ].join('; ')));
        }, timeout);

        function tearDown() {
          clearTimeout(timer);
          child.stdout.removeListener('data', checkOutput);
          child.removeListener('exit', onChildExit);
        }

        child.on('exit', onChildExit);
        child.stdout.on('data', checkOutput);
        checkOutput();
      });
    },

    waitForPrompt(timeout = 2000) {
      return this.waitFor(/>\s+$/, timeout);
    },

    ctrlC() {
      return this.command('.interrupt');
    },

    get output() {
      return getOutput();
    },

    get rawOutput() {
      return outputBuffer.join('').toString();
    },

    parseSourceLines() {
      return getOutput().split('\n')
        .map((line) => line.match(/(?:\*|>)?\s*(\d+)/))
        .filter((match) => match !== null)
        .map((match) => +match[1]);
    },

    command(input) {
      this.flushOutput();
      child.stdin.write(input);
      child.stdin.write('\n');
      return this.waitForPrompt();
    },

    stepCommand(input) {
      this.flushOutput();
      child.stdin.write(input);
      child.stdin.write('\n');
      return this
        .waitFor(
          /(?:assert|break|debugCommand|exception|other|promiseRejection) in/
        )
        .then(() => this.waitForPrompt());
    },

    quit() {
      return new Promise((resolve) => {
        child.stdin.end();
        child.on('exit', resolve);
      });
    },
  };
}
module.exports = startCLI;
