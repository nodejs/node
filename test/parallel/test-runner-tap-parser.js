'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('assert');

const { TapParser } = require('internal/test_runner/tap_parser');

function TAPParser(input) {
  const parser = new TapParser();
  const ast = parser.parse(input);
  return ast;
}

// TAP version

{
  const ast = TAPParser('TAP version 14');
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
  // assert.throws(() => TAPParser('TAP version'), {
  //   name: 'SyntaxError',
  //   message:
  //     'Expected a version number, received "EOF" (EOF) at line 1, column 12 (start 12, end 12)',
  // });
}

{
  // assert.throws(() => TAPParser('TAP'), {
  //   name: 'SyntaxError',
  //   message:
  //     'Expected "version" keyword, received "EOF" (EOF) at line 1, column 4 (start 4, end 4)',
  // });
}

// Test plan

{
  const ast = TAPParser('1..5 # reason');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [{ plan: { start: '1', end: '5', reason: 'reason' } }],
    },
  });
}

{
  const ast = TAPParser(
    '1..5 # reason "\\ !"\\#$%&\'()*+,\\-./:;<=>?@[]^_`{|}~'
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
  // assert.throws(() => TAPParser('1..'), {
  //   name: 'SyntaxError',
  //   message:
  //     'Expected a plan end count, received "EOF" (EOF) at line 1, column 4 (start 4, end 4)',
  // });
}

{
  // assert.throws(() => TAPParser('1..abc'), {
  //   name: 'SyntaxError',
  //   message:
  //     'Expected ".." symbol, received "..abc" (Literal) at line 1, column 2 (start 1, end 5)',
  // });
}

{
  // assert.throws(() => TAPParser('1..-1'), {
  //   name: 'SyntaxError',
  //   message:
  //     'Expected a plan end count, received "-" (Dash) at line 1, column 4 (start 3, end 3)',
  // });
}

{
  // assert.throws(() => TAPParser('1.1'), {
  //   name: 'SyntaxError',
  //   message:
  //     'Expected ".." symbol, received "." (Literal) at line 1, column 2 (start 1, end 1)',
  // });
}

// Test point

{
  const ast = TAPParser('ok');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: false,
              },
              description: '',
              reason: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser('not ok');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '',
              status: {
                pass: false,
                fail: true,
                skip: false,
                todo: false,
              },
              description: '',
              reason: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser('ok 1');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '1',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: false,
              },
              description: '',
              reason: '',
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
              id: '1',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: false,
              },
              description: '',
              reason: '',
            },
            {
              id: '2',
              status: {
                pass: false,
                fail: true,
                skip: false,
                todo: false,
              },
              description: '',
              reason: '',
            },
          ],
        },
      ],
    },
  });
}

{
  // A subtest is indented by 4 spaces
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
              id: '1',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: false,
              },
              description: '',
              reason: '',
            },
          ],
          documents: [
            {
              tests: [
                {
                  id: '1',
                  status: {
                    pass: true,
                    fail: false,
                    skip: false,
                    todo: false,
                  },
                  description: '',
                  reason: '',
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
  const ast = TAPParser('ok 1 description');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '1',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: false,
              },
              description: 'description',
              reason: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser('ok 1 - description');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '1',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: false,
              },
              description: 'description',
              reason: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser('ok 1 - description # todo');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '1',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: true,
              },
              description: 'description',
              reason: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser('ok 1 - description \\# todo');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '1',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: false,
              },
              description: 'description # todo',
              reason: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser('ok 1 - description \\ # todo');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '1',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: true,
              },
              description: 'description',
              reason: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser(
    'ok 1 description \\# \\\\ world # TODO escape \\# characters with \\\\'
  );
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '1',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: true,
              },
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
  const ast = TAPParser('ok 1 - description # ##');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '1',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: false,
              },
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
  const ast = TAPParser(
    'ok 2 not skipped: https://example.com/page.html#skip is a url'
  );
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '2',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: false,
              },
              description:
                'not skipped: https://example.com/page.html#skip is a url',
              reason: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser('ok 3 - #SkIp case insensitive, so this is skipped');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '3',
              status: {
                pass: true,
                fail: false,
                skip: true,
                todo: false,
              },
              reason: 'case insensitive, so this is skipped',
              description: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser('ok ok ok');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: false,
              },
              description: 'ok ok',
              reason: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser('ok not ok');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: false,
              },
              description: 'not ok',
              reason: '',
            },
          ],
        },
      ],
    },
  });
}

{
  const ast = TAPParser('ok 1..1');
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              id: '1',
              status: {
                pass: true,
                fail: false,
                skip: false,
                todo: false,
              },
              description: '', // This looks like an edge case
              reason: '',
            },
          ],
        },
      ],
    },
  });
}

// Comment

{
  const ast = TAPParser('# comment');
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
  const ast = TAPParser('#');
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
  const ast = TAPParser('####');
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
  // Note the leading 2 valid spaces
  const ast = TAPParser(`
  ---
  message: 'description'
  property: 'value'
  ...
`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              diagnostics: ["message: 'description'", "property: 'value'"],
            },
          ],
        },
      ],
    },
  });
}

{
  // Note the leading 2 valid spaces
  const ast = TAPParser(`
  ---
  message: "Board layout"
  severity: comment
  dump:
    board:
      - '      16G         05C        '
      - '      G N C       C C G      '
      - '        G           C  +     '
      - '10C   01G         03C        '
      - 'R N G G A G       C C C      '
      - '  R     G           C  +     '
      - '      01G   17C   00C        '
      - '      G A G G N R R N R      '
      - '        G     R     G        '
  ...
`);
  assert.deepStrictEqual(ast, {
    root: {
      documents: [
        {
          tests: [
            {
              diagnostics: [
                'message: "Board layout"',
                'severity: comment',
                'dump:',
                '  board:',
                "    - '      16G         05C        '",
                "    - '      G N C       C C G      '",
                "    - '        G           C  +     '",
                "    - '10C   01G         03C        '",
                "    - 'R N G G A G       C C C      '",
                "    - '  R     G           C  +     '",
                "    - '      01G   17C   00C        '",
                "    - '      G A G G N R R N R      '",
                "    - '        G     R     G        '",
              ],
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
//   assert.throws(
//     () =>
//       TAPParser(
//         `
//   message: 'description'
//   property: 'value'
//   ...
// `
//       ),
//     {
//       name: 'SyntaxError',
//       message:
//         'Unexpected YAML end marker, received "..." (YamlEndKeyword) at line 4, column 3 (start 48, end 50)',
//     }
//   );
}

{
//   assert.throws(
//     () =>
//       TAPParser(
//         `
//   ---
//   message: 'description'
//   property: 'value'
// `
//       ),
//     {
//       name: 'SyntaxError',
//       message:
//         'Expected end of YAML block, received "\'value\'" (Literal) at line 4, column 13 (start 44, end 50)',
//     }
//   );
}

{
//   assert.throws(
//     () =>
//       // Note the leading 3 spaces before ---
//       TAPParser(
//         `
//    ---
//   message: 'description'
//   property: 'value'
//   ...
// `
//       ),
//     {
//       name: 'SyntaxError',
//       message:
//         'Expected valid YAML indentation (2 spaces), received 3 spaces at line 2, column 3 (start 3, end 3)',
//     }
//   );
}

{
//   assert.throws(
//     () =>
//       // Note the leading 5 spaces before ---
//       TAPParser(
//         `
//      ---
//   message: 'description'
//   property: 'value'
//   ...
// `
//       ),
//     {
//       name: 'SyntaxError',
//       message:
//         'Expected valid YAML indentation (2 spaces), received 5 spaces at line 2, column 5 (start 5, end 5)',
//     }
//   );
}

{
//   assert.throws(
//     () =>
//       // Note the leading 4 spaces before ---
//       TAPParser(
//         `
//     ---
//   message: 'description'
//   property: 'value'
//   ...
// `
//       ),
//     {
//       name: 'SyntaxError',
//       message:
//         'Expected a valid token, received "---" (YamlStartKeyword) at line 2, column 5 (start 5, end 7)',
//     }
//   );
}

{
//   assert.throws(
//     () =>
//       // Note the leading 4 spaces before ...
//       TAPParser(
//         `
//   ---
//   message: 'description'
//   property: 'value'
//     ...
// `
//       ),
//     {
//       name: 'SyntaxError',
//       message:
//         'Expected end of YAML block, received "..." (YamlEndKeyword) at line 5, column 5 (start 56, end 58)',
//     }
//   );
}

// Pragma

{
  const ast = TAPParser('pragma +strict, -warnings');
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
  const ast = TAPParser('Bail out! Error');
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
  // assert.throws(() => TAPParser('abc'), {
  //   name: 'SyntaxError',
  //   message:
  //     'Expected a valid token, received "abc" (Literal) at line 1, column 1 (start 0, end 2)',
  // });
}

{
  // assert.throws(() => TAPParser('    abc'), {
  //   name: 'SyntaxError',
  //   message:
  //     'Expected a valid token, received "abc" (Literal) at line 1, column 5 (start 4, end 6)',
  // });
}
