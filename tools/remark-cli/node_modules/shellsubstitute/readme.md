# Shell Substitute [![npm version](https://img.shields.io/npm/v/shellsubstitute.svg)](https://www.npmjs.com/package/shellsubstitute) [![npm](https://img.shields.io/npm/dm/shellsubstitute.svg)](https://www.npmjs.com/package/shellsubstitute) [![Build Status](https://travis-ci.org/featurist/shellsubstitute.svg?branch=master)](https://travis-ci.org/featurist/shellsubstitute)

Like shell substitution but for your JS.

    var substitute = require('shellsubstitute');

    substitute('Hi $USER', {USER: 'Josh'}) // Hi Josh
    substitute('Hi ${USER}', {USER: 'Josh'}) // Hi Josh

    // escape
    substitute('Hi \\$USER', {USER: 'Josh'}) // Hi $USER
    substitute('Hi \\${USER}', {USER: 'Josh'}) // Hi ${USER}

    // escape escapes
    substitute('Hi \\\\$USER', {USER: 'Josh'}) // Hi \$USER
    substitute('Hi \\\\${USER}', {USER: 'Josh'}) // Hi \${USER}

## Syntax

Variables are `$` followed by `_` or numbers `0-9` or upper or lower-case characters `a-z`.

Variables can be wrapped in braces `{...}`. Useful to delimit the variable from following text.
