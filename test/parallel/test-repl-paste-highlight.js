// Flags: --expose-internals
'use strict';

// Pasted text reaches the REPL as a single input chunk containing many
// characters. With syntax highlighting enabled, check that such a chunk leaves
// the correct line buffer, highlighted rendering and evaluated result.

const common = require('../common');
const assert = require('assert');
const { highlight } = require('internal/repl/highlight');
const { startNewREPLServer } = require('../common/repl');

common.skipIfInspectorDisabled();

// Everything written since the most recent full redraw of the input.
function lastRedraw(rendered) {
  const clearScreenDown = '\x1b[0J';
  return rendered.slice(rendered.lastIndexOf(clearScreenDown) + clearScreenDown.length);
}

// The expected rendering is built with the REPL's own highlighter on purpose.
// This test is not about which colors highlight() picks (that is covered by
// test-repl-highlight.js); whatever it produces is taken as correct. The point
// is that a pasted chunk ends up rendered exactly as highlight() renders the
// same text when it is typed, so changes to the highlighting style do not
// require changes here.
function expectedRendering(line) {
  const [first, ...rest] = line.split('\n');
  return `> ${highlight(first)}` + rest.map((l) => `\n| ${highlight(l)}`).join('');
}

// Pastes `chunk` as one 'data' event, checks the resulting line and its
// rendering, then submits the line and returns the REPL context.
async function paste(chunk, expectedLine) {
  const { replServer, output, run } = startNewREPLServer({
    prompt: '> ',
    useColors: true,
  });
  output.accumulator = '';

  await run(chunk);
  assert.strictEqual(replServer.line, expectedLine);
  const rendered = lastRedraw(output.accumulator);
  assert.ok(
    rendered.startsWith(expectedRendering(expectedLine)),
    `unexpected rendering: ${JSON.stringify(rendered)}`,
  );

  await run('\r');
  replServer.close();
  return replServer.context;
}

function createObject(i) {
  return {
    id: i,
    name: `item-${i}`,
    tags: ['alpha', 'beta', 'gamma'],
    nested: { enabled: true, ratio: 0.5, label: 'a string value' },
  };
}

(async () => {
  // Large multi-line object.
  {
    const objects = [];
    for (let i = 0; i < 16; i++) objects.push(createObject(i));
    const lines = `x = ${JSON.stringify(objects, null, 2)}`.split('\n');
    const context = await paste(lines.join('\r'), lines.join('\n'));
    assert.strictEqual(JSON.stringify(context.x), JSON.stringify(objects));
  }

  // Long single line.
  {
    const items = [];
    for (let i = 0; i < 100; i++) {
      items.push(`{ id: ${i}, name: 'item-${i}', tags: ['a', 'b'] }`);
    }
    const line = `y = [${items.join(', ')}]`;
    const context = await paste(line, line);
    assert.strictEqual(context.y.length, 100);
    assert.strictEqual(context.y[99].name, 'item-99');
  }
})().then(common.mustCall());
