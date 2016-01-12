[![NPM version](https://img.shields.io/npm/v/esprima.svg)](https://www.npmjs.com/package/esprima)
[![npm download](https://img.shields.io/npm/dm/esprima.svg)](https://www.npmjs.com/package/esprima)
[![Build Status](https://img.shields.io/travis/jquery/esprima/master.svg)](https://travis-ci.org/jquery/esprima)
[![Coverage Status](https://img.shields.io/codecov/c/github/jquery/esprima/master.svg)](https://codecov.io/github/jquery/esprima)

**Esprima** ([esprima.org](http://esprima.org), BSD license) is a high performance,
standard-compliant [ECMAScript](http://www.ecma-international.org/publications/standards/Ecma-262.htm)
parser written in ECMAScript (also popularly known as
[JavaScript](https://en.wikipedia.org/wiki/JavaScript)).
Esprima is created and maintained by [Ariya Hidayat](https://twitter.com/ariyahidayat),
with the help of [many contributors](https://github.com/jquery/esprima/contributors).

### Features

- Full support for ECMAScript 6 ([ECMA-262](http://www.ecma-international.org/publications/standards/Ecma-262.htm))
- Sensible [syntax tree format](https://github.com/estree/estree/blob/master/spec.md) as standardized by [ESTree project](https://github.com/estree/estree)
- Optional tracking of syntax node location (index-based and line-column)
- [Heavily tested](http://esprima.org/test/ci.html) (~1250 [unit tests](https://github.com/jquery/esprima/tree/master/test/fixtures) with [full code coverage](https://codecov.io/github/jquery/esprima))

Esprima serves as a **building block** for some JavaScript
language tools, from [code instrumentation](http://esprima.org/demo/functiontrace.html)
to [editor autocompletion](http://esprima.org/demo/autocomplete.html).

Esprima runs on many popular web browsers, as well as other ECMAScript platforms such as
[Rhino](http://www.mozilla.org/rhino), [Nashorn](http://openjdk.java.net/projects/nashorn/), and [Node.js](https://npmjs.org/package/esprima).

For more information, check the web site [esprima.org](http://esprima.org).
