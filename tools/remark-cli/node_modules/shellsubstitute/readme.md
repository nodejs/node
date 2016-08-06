# Shell Substitute

Like shell substitution but for your JS.

    var substitute = require('shellsubstitute');

    substitute('Hi $USER', {USER: 'Josh'}) // Hi Josh
    substitute('Hi ${USER}', {USER: 'Josh'}) // Hi Josh

    // escape
    substitute('Hi \\$USER', {USER: 'Josh'}) // Hi $USER
    substitute('Hi \\${USER}', {USER: 'Josh'}) // Hi ${USER}

## Syntax

Variables are `$` followed by `_` or numbers `0-9` or upper or lower-case characters `a-z`.

Variables can be wrapped in braces `{...}`. Useful to delimit the variable from following text.
