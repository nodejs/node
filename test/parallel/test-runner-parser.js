'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('assert');

const { TapParser } = require('internal/test_runner/tap_parser');

function TAPParser(input) {
  const parser = new TapParser(input, { debug: true });
  return parser.parse();
}

// TAP version

{
  const ast = TAPParser(`TAP version 14`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          version: '14',
        },
      ],
    },
  });
}

{
  assert.throws(() => TAPParser(`TAP version`), {
    name: 'SyntaxError',
    message:
      'Expected Numeric, got "EOF" at line 1, column 12 (start 12, end 12)',
  });
}

{
  assert.throws(() => TAPParser(`TAP`), {
    name: 'SyntaxError',
    message:
      'Expected "version", got "EOF" at line 1, column 4 (start 4, end 4)',
  });
}

// Test plan

{
  const ast = TAPParser(`1..5 # reason`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [{ plan: { start: '1', end: '5', reason: 'reason' } }],
    },
  });
}

{
  const ast = TAPParser(
    `1..5 # reason "\\ !"\\#$%&'()*+,\\-./:;<=>?@[]^_\`{|}~`
  );
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          plan: {
            start: '1',
            end: '5',
            reason: 'reason " !"\\#$%&\'()*+,-./:;<=>?@[]^_`{|}~',
          },
        },
      ],
    },
  });
}

{
  assert.throws(() => TAPParser(`1..`), {
    name: 'SyntaxError',
    message:
      'Expected a Numeric, got "EOF" at line 1, column 4 (start 4, end 4)',
  });
}

{
  assert.throws(() => TAPParser(`1..abc`), {
    name: 'SyntaxError',
    message: 'Expected "..", got "..abc" at line 1, column 2 (start 1, end 5)',
  });
}

{
  assert.throws(() => TAPParser(`1..-1`), {
    name: 'SyntaxError',
    message: 'Expected a Numeric, got "-" at line 1, column 4 (start 3, end 3)',
  });
}

{
  assert.throws(() => TAPParser(`1.1`), {
    name: 'SyntaxError',
    message: 'Expected "..", got "." at line 1, column 2 (start 1, end 1)',
  });
}

// Test point

{
  const ast = TAPParser(`ok`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'passed',
              id: '90001',
              description: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`not ok`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'failed',
              id: '90001',
              description: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`ok 1`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'passed',
              id: '1',
              description: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`
ok 1
not ok 2
`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'passed',
              id: '1',
            },
            {
              status: 'failed',
              id: '2',
            },
          ],
        },
      ],
    },
  });
}

{
  // a subtest is indented by 4 spaces
  const ast = TAPParser(`
ok 1
    ok 1
`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'passed',
              id: '1',
            },
          ],
          documents: [
            {
              tests: [
                {
                  status: 'passed',
                  id: '1',
                },
              ],
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`ok 1 description`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'passed',
              id: '1',
              description: 'description',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`ok 1 - description`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'passed',
              id: '1',
              description: 'description',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`ok 1 - description # todo`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'todo',
              id: '1',
              description: 'description',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`ok 1 - description \\# todo`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'passed',
              id: '1',
              description: 'description # todo',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`ok 1 - description \\ # todo`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'passed',
              id: '1',
              description: 'description  # todo',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(
    `ok 1 description \\# \\\\ world # TODO escape \\# characters with \\\\`
  );
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'todo',
              id: '1',
              description: 'description # \\ world',
              reason: 'escape # characters with \\',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`ok 1 - description # ##`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'passed',
              id: '1',
              description: 'description',
              reason: '##',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`ok ok ok`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'passed',
              id: '90001',
              description: 'ok ok',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`ok not ok`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'passed',
              id: '90001',
              description: 'not ok',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`ok 1..1`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              status: 'passed',
              id: '1',
              description: '', // this looks like an edge case
            },
          ],
        },
      ],
    },
  });
}

// Comment

{
  const ast = TAPParser(`# comment`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          comments: ['comment'],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`#`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          comments: [''],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`####`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          comments: ['###'],
        },
      ],
    },
  });
}

// Diagnostic

{
  // note the leading 2 valid spaces
  const ast = TAPParser(`
  ---
  message: "description"
  property: 'value'
  ...
`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              diagnostics: ['message: "description"', "property: 'value'"],
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(`
  ---
  ...
`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              diagnostics: [],
            },
          ],
        },
      ],
    },
  });
}

{
  assert.throws(
    () =>
      TAPParser(
        `
  message: "description"
  property: 'value'
  ...
`
      ),
    {
      name: 'SyntaxError',
      message:
        'Unexpected end of YAML block, got "..." at line 4, column 3 (start 48, end 50)',
    }
  );
}

{
  assert.throws(
    () =>
      TAPParser(
        `
  ---
  message: "description"
  property: 'value'
`
      ),
    {
      name: 'SyntaxError',
      message: `Expected end of YAML block, got "'value'" at line 4, column 13 (start 44, end 50)`,
    }
  );
}

{
  assert.throws(
    () =>
      // note the leading 3 spaces before ---
      TAPParser(
        `
   ---
  message: "description"
  property: 'value'
  ...
`
      ),
    {
      name: 'SyntaxError',
      message: `Invalid indentation level. Expected 2 spaces, got " " at line 2, column 3 (start 3, end 3)`,
    }
  );
}

{
  assert.throws(
    () =>
      // note the leading 5 spaces before ---
      TAPParser(
        `
     ---
  message: "description"
  property: 'value'
  ...
`
      ),
    {
      name: 'SyntaxError',
      message: `Invalid indentation level. Expected 4 spaces, got " " at line 2, column 5 (start 5, end 5)`,
    }
  );
}

{
  assert.throws(
    () =>
      // note the leading 4 spaces before ---
      TAPParser(
        `
    ---
  message: "description"
  property: 'value'
  ...
`
      ),
    {
      name: 'SyntaxError',
      message: `Expected a valid token, got "---" at line 2, column 5 (start 5, end 7)`,
    }
  );
}

{
  assert.throws(
    () =>
      // note the leading 4 spaces before ...
      TAPParser(
        `
  ---
  message: "description"
  property: 'value'
    ...
`
      ),
    {
      name: 'SyntaxError',
      message: `Expected a valid token, got "..." at line 5, column 5 (start 56, end 58)`,
    }
  );
}

// Pragma

{
  const ast = TAPParser(`pragma +strict, -warnings`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          pragmas: {
            strict: true,
            warnings: false,
          },
        },
      ],
    },
  });
}

// Bail out

{
  const ast = TAPParser(`Bail out! Error`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          bailout: 'Error',
        },
      ],
    },
  });
}

// Non-recognized

{
  assert.throws(() => TAPParser(`abc`), {
    name: 'SyntaxError',
    message:
      'Expected a valid token, got "abc" at line 1, column 1 (start 0, end 2)',
  });
}

{
  assert.throws(() => TAPParser(`    abc`), {
    name: 'SyntaxError',
    message:
      'Expected a valid token, got "abc" at line 1, column 5 (start 4, end 6)',
  });
}
