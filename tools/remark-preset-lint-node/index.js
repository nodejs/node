// @see https://github.com/nodejs/node/blob/master/doc/STYLE_GUIDE.md

'use strict';

module.exports.plugins = [
  require('remark-lint'),
  require('remark-lint-checkbox-content-indent'),
  require('remark-lint-definition-spacing'),
  require('remark-lint-fenced-code-flag'),
  require('remark-lint-final-definition'),
  require('remark-lint-final-newline'),
  require('remark-lint-hard-break-spaces'),
  require('remark-lint-no-auto-link-without-protocol'),
  require('remark-lint-no-blockquote-without-caret'),
  require('remark-lint-no-duplicate-definitions'),
  require('remark-lint-no-file-name-articles'),
  require('remark-lint-no-file-name-consecutive-dashes'),
  require('remark-lint-no-file-name-outer-dashes'),
  require('remark-lint-no-heading-content-indent'),
  require('remark-lint-no-heading-indent'),
  require('remark-lint-no-inline-padding'),
  require('remark-lint-no-multiple-toplevel-headings'),
  require('remark-lint-no-shell-dollars'),
  require('remark-lint-no-shortcut-reference-image'),
  require('remark-lint-no-table-indentation'),
  require('remark-lint-no-tabs'),
  require('remark-lint-no-unused-definitions'),
  require('remark-lint-rule-style'),
  require('remark-lint-table-pipes'),
  [require('remark-lint-blockquote-indentation'), 2],
  [
    require('remark-lint-checkbox-character-style'),
    {
      'checked': 'x', 'unchecked': ' '
    }
  ],
  [require('remark-lint-code-block-style'), 'fenced'],
  [require('remark-lint-fenced-code-marker'), '`'],
  [require('remark-lint-file-extension'), 'md'],
  [require('remark-lint-first-heading-level'), 1],
  [require('remark-lint-heading-style'), 'atx'],
  [
    require('remark-lint-prohibited-strings'),
    [
      { no: 'Github', yes: 'GitHub' },
      { no: 'Javascript', yes: 'JavaScript' },
      { no: 'Node.JS', yes: 'Node.js' },
      { no: 'v8', yes: 'V8' }
    ]
  ],
  [require('remark-lint-strong-marker'), '*'],
  [require('remark-lint-table-cell-padding'), 'padded']
];
