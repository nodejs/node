import foo from 'foo';
console.log(foo());

import bar from 'foo/bar';
console.log(bar());

import baz from 'baz';
console.log(baz());

import qux from 'data:text/javascript,export default () => \'bad\'';
console.log(qux());
