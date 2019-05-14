# qw

Quoted word literals!

```js
const qw = require('qw')

const myword = qw` this is
  a long list
  of words`
// equiv of:
const myword = [ 'this', 'is', 'a', 'long', 'list', 'of', 'words' ]
```

You can embed vars in the usual way:

```js
const mywords = qw`product ${23 * 5} also ${'escaping a string'}`
// equiv of:
const mywords = [ 'product', 23 * 5, 'also', 'escaping a string' ]
```

You can also embed vars inside strings:

```js
const mywords = qw`product=${23 * 5} also "${'escaping a string'}"`
// equiv of:
const mywords = [ 'product=' + (23 * 5), 'also', '"escaping a string"' ]
```

## DESCRIPTION

This uses template strings to bring over this little common convenience from
Perl-land.
