'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const { describe, it } = require('node:test');

common.skipIfInspectorDisabled();

const repl = require('repl');

const testingReplPrompt = '_REPL_TESTING_PROMPT_>';

// Processes some input in a REPL instance and returns a promise that
// resolves to the produced output (as a string).
function getReplRunOutput(input, replOptions) {
  return new Promise((resolve) => {
    const inputStream = new ArrayStream();
    const outputStream = new ArrayStream();

    const replServer = repl.start({
      input: inputStream,
      output: outputStream,
      prompt: testingReplPrompt,
      ...replOptions,
    });

    let output = '';

    outputStream.write = (chunk) => {
      output += chunk;
      // The prompt appears after the input has been processed
      if (output.includes(testingReplPrompt)) {
        replServer.close();
        resolve(output);
      }
    };

    inputStream.emit('data', input);

    inputStream.run(['']);
  });
}

describe('with previews', () => {
  it("doesn't show previews by default", async () => {
    const input = "'Hello custom' + ' eval World!'";
    const output = await getReplRunOutput(
      input,
      {
        terminal: true,
        eval: (code, _ctx, _replRes, cb) => cb(null, eval(code)),
      },
    );
    const lines = getSingleCommandLines(output);
    assert.match(lines.command, /^'Hello custom' \+ ' eval World!'/);
    assert.match(lines.prompt, new RegExp(`${testingReplPrompt}$`));
    assert.strictEqual(lines.result, "'Hello custom eval World!'");
    assert.strictEqual(lines.preview, undefined);
  });

  it('does show previews if `preview` is set to `true`', async () => {
    const input = "'Hello custom' + ' eval World!'";
    const output = await getReplRunOutput(
      input,
      {
        terminal: true,
        eval: (code, _ctx, _replRes, cb) => cb(null, eval(code)),
        preview: true,
      },
    );
    const lines = getSingleCommandLines(output);
    assert.match(lines.command, /^'Hello custom' \+ ' eval World!'/);
    assert.match(lines.prompt, new RegExp(`${testingReplPrompt}$`));
    assert.strictEqual(lines.result, "'Hello custom eval World!'");
    assert.match(lines.preview, /'Hello custom eval World!'/);
  });
});

function getSingleCommandLines(output) {
  const outputLines = output.split('\n');

  // The first line contains the command being run
  const command = outputLines.shift();

  // The last line contains the prompt (asking for some new input)
  const prompt = outputLines.pop();

  // The line before the last one contains the result of the command
  const result = outputLines.pop();

  // The line before that contains the preview of the command
  const preview = outputLines.shift();

  return {
    command,
    prompt,
    result,
    preview,
  };
}
