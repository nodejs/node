# falafel

Transform the [ast](http://en.wikipedia.org/wiki/Abstract_syntax_tree) on a
recursive walk.

[![browser support](http://ci.testling.com/substack/node-falafel.png)](http://ci.testling.com/substack/node-falafel)

[![build status](https://secure.travis-ci.org/substack/node-falafel.png)](http://travis-ci.org/substack/node-falafel)

This modules uses [acorn](https://npmjs.org/package/acorn) to create an AST from
source code.

![falafel döner](http://substack.net/images/falafel.png)

# example

## array.js

Put a function wrapper around all array literals.

``` js
var falafel = require('falafel');

var src = '(' + function () {
    var xs = [ 1, 2, [ 3, 4 ] ];
    var ys = [ 5, 6 ];
    console.dir([ xs, ys ]);
} + ')()';

var output = falafel(src, function (node) {
    if (node.type === 'ArrayExpression') {
        node.update('fn(' + node.source() + ')');
    }
});
console.log(output);
```

output:

```
(function () {
    var xs = fn([ 1, 2, fn([ 3, 4 ]) ]);
    var ys = fn([ 5, 6 ]);
    console.dir(fn([ xs, ys ]));
})()
```

# methods

``` js
var falafel = require('falafel')
```

## falafel(src, opts={}, fn)

Transform the string source `src` with the function `fn`, returning a
string-like transformed output object.

For every node in the ast, `fn(node)` fires. The recursive walk is a
pre-traversal, so children get called before their parents.

Performing a pre-traversal makes it easier to write nested transforms since
transforming parents often requires transforming all its children first.

The return value is string-like (it defines `.toString()` and `.inspect()`) so
that you can call `node.update()` asynchronously after the function has
returned and still capture the output.

Instead of passing a `src` you can also use `opts.source`.

All of the `opts` will be passed directly to
[acorn](https://npmjs.org/package/acorn).

## custom parser

You may pass in an instance of acorn to the opts as `opts.parser` to use that
version instead of the version of acorn packaged with this library.

```js
var acorn = require('acorn-jsx');

falafel(src, {parser: acorn, plugins: { jsx: true }}, function(node) {
  // this will parse jsx
});
```

# nodes

Aside from the regular [esprima](http://esprima.org) data, you can also call
some inserted methods on nodes.

Aside from updating the current node, you can also reach into sub-nodes to call
update functions on children from parent nodes.

## node.source()

Return the source for the given node, including any modifications made to
children nodes.

## node.update(s)

Transform the source for the present node to the string `s`.

Note that in `'ForStatement'` node types, there is an existing subnode called
`update`. For those nodes all the properties are copied over onto the
`node.update()` function.

## node.parent

Reference to the parent element or `null` at the root element.

# install

With [npm](http://npmjs.org) do:

```
npm install falafel
```

# license

MIT
