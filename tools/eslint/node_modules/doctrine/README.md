doctrine ([doctrine](http://github.com/Constellation/doctrine)) is JSDoc parser.

[![Build Status](https://travis-ci.org/Constellation/doctrine.svg?branch=master)](https://travis-ci.org/Constellation/doctrine)
[![Coverage Status](https://img.shields.io/coveralls/Constellation/doctrine.svg)](https://coveralls.io/r/Constellation/doctrine?branch=master)
[![Dependency Status](https://david-dm.org/Constellation/doctrine.svg)](https://david-dm.org/Constellation/doctrine)
[![devDependency Status](https://david-dm.org/Constellation/doctrine/dev-status.svg)](https://david-dm.org/Constellation/doctrine#info=devDependencies)
[![Gitter chat](https://badges.gitter.im/Constellation/doctrine.png)](https://gitter.im/Constellation/doctrine)

It is now used by content assist system of [Eclipse Orion](http://www.eclipse.org/orion/) ([detail](http://planetorion.org/news/2012/10/orion-1-0-release/)). And used as JSDoc validator in [ESLint](http://eslint.org/).

Doctrine can be used in a web browser with using browserify.
or in a Node.js application via the package manager:

    npm install doctrine

simple example:

    doctrine.parse(
        [
            "/**",
            " * This function comment is parsed by doctrine",
            " * @param {{ok:String}} userName",
            "*/"
        ].join('\n'), { unwrap: true });

and gets following information

    {
        "description": "This function comment is parsed by doctrine",
        "tags": [
            {
                "title": "param",
                "description": null,
                "type": {
                    "type": "RecordType",
                    "fields": [
                        {
                            "type": "FieldType",
                            "key": "ok",
                            "value": {
                                "type": "NameExpression",
                                "name": "String"
                            }
                        }
                    ]
                },
                "name": "userName"
            }
        ]
    }

see [demo page](http://constellation.github.com/doctrine/demo/index.html) more detail.

### Options

#### doctrine.parse
We can pass options to `doctrine.parse(comment, options)`.
```js
{
    unwrap: boolean,          // default: false
    tags: [ string ] | null,  // default: null
    recoverable: boolean,     // default: false
    sloppy: boolean,          // default: false
    lineNumbers: boolean      // default: false
}
```

##### unwrap

When `unwrap` is `true`, doctrine attempt to unwrap comment specific string from a provided comment text. (removes `/**`, `*/` and `*`)
For example, `unwrap` transforms
```
/**
 * @param use
 */
```
to
```
@param use
```
If a provided comment has these comment specific strings, you need to specify this `unwrap` option to `true`.

##### tags

When `tags` array is specified, doctrine only produce tags that is specified in this array.
For example, if you specify `[ 'param' ]`, doctrine only produces `param` tags.
If null is specified, doctrine produces all tags that doctrine can recognize.

##### recoverable

When `recoverable` is `true`, doctrine becomes `recoverable` - When failing to parse jsdoc comment, doctrine recovers its state and attempt to continue parsing.

##### sloppy

When `sloppy` is `true`,
```
@param String [foo]
```
's `[foo]` is interpreted as a optional parameter, not interpreted as a name of this `@param`.

##### lineNumbers

When `lineNumbers` is `true`, parsed tags will include a `lineNumber` property indicating the line (relative to the start of the comment block) where each tag is located in the source. So, given the following comment:
```
/**
 * @param {String} foo
 * @return {number}
 */
```
The `@param` tag will have `lineNumber: 1`, and the `@return` tag will have `lineNumber: 2`.


### License

#### doctrine

Copyright (C) 2012 [Yusuke Suzuki](http://github.com/Constellation)
 (twitter: [@Constellation](http://twitter.com/Constellation)) and other contributors.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#### esprima

some of functions is derived from esprima

Copyright (C) 2012, 2011 [Ariya Hidayat](http://ariya.ofilabs.com/about)
 (twitter: [@ariyahidayat](http://twitter.com/ariyahidayat)) and other contributors.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#### closure-compiler

some of extensions is derived from closure-compiler

Apache License
Version 2.0, January 2004
http://www.apache.org/licenses/
