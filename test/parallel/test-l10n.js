'use strict';

// Flags: --expose-internals

const common = require('../common');
const l10n = require('internal/l10n');
const assert = require('assert');

// right now we only support english

if (l10n.icu) {
// ICU is present, the strings will be localized
  assert.equal('English Testing', l10n('TEST', 'Foo'));
  assert.equal('ROOT abc 1', l10n('TEST2', 'Foo', 'abc', 1));
  assert.equal('Fallback', l10n('TEST3', 'Fallback'));
} else {
// ICU is not present, the fallbacks will be used
  assert.equal('Foo', l10n('TEST', 'Foo'));
  assert.equal('Foo abc 1', l10n('TEST2', 'Foo', 'abc', 1));
  assert.equal('Fallback', l10n('TEST3', 'Fallback'));
}
