'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('assert');

const { TapParser } = require('internal/test_runner/tap_parser');

function TAPParser(input) {
  const parser = new TapParser(input, { debug: false });
  return parser.parse();
}

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
  // note the leading 2 spaces!
  const ast = TAPParser(`
  ---
    message: "description"
    severity: fail
  ...
`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              diagnostics: [
                '    message: "description"',
                '    severity: fail',
                '  ',
              ],
            },
          ],
        },
      ],
    },
  });
}

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
