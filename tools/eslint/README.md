[![NPM version][npm-image]][npm-url]
[![build status][travis-image]][travis-url]
[![Test coverage][coveralls-image]][coveralls-url]
[![Downloads][downloads-image]][downloads-url]
[![Bountysource](https://www.bountysource.com/badge/tracker?tracker_id=282608)](https://www.bountysource.com/trackers/282608-eslint?utm_source=282608&utm_medium=shield&utm_campaign=TRACKER_BADGE)

# ESLint

[Website](http://eslint.org) | [Configuring](http://eslint.org/docs/user-guide/configuring) | [Rules](http://eslint.org/docs/rules/) | [Contributing](http://eslint.org/docs/developer-guide/contributing) | [Twitter](https://twitter.com/geteslint) | [Mailing List](https://groups.google.com/group/eslint)

ESLint is a tool for identifying and reporting on patterns found in ECMAScript/JavaScript code. In many ways, it is similar to JSLint and JSHint with a few exceptions:

* ESLint uses [Espree](https://github.com/eslint/espree) for JavaScript parsing.
* ESLint uses an AST to evaluate patterns in code.
* ESLint is completely pluggable, every single rule is a plugin and you can add more at runtime.

## Installation

You can install ESLint using npm:

    npm install -g eslint

## Usage

    eslint test.js test2.js

## Frequently Asked Questions

### Why don't you like JSHint???

I do like JSHint. And I like Anton and Rick. Neither of those were deciding factors in creating this tool. The fact is that I've had a dire need for a JavaScript tool with pluggable linting rules. I had hoped JSHint would be able to do this, however after chatting with Anton, I found that the planned plugin infrastructure wasn't going to suit my purpose.

### I'm not giving up JSHint for this!

That's not really a question, but I got it. I'm not trying to convince you that ESLint is better than JSHint. The only thing I know is that ESLint is better than JSHint for what I'm doing. In the off chance you're doing something similar, it might be better for you. Otherwise, keep using JSHint, I'm certainly not going to tell you to stop using it.

### How does ESLint performance compare to JSHint?

ESLint is slower than JSHint, usually 2-3x slower on a single file. This is because ESLint uses Espree to construct an AST before it can evaluate your code whereas JSHint evaluates your code as it's being parsed. The speed is also based on the number of rules you enable; the more rules you enable, the slower the process.

Despite being slower, we believe that ESLint is fast enough to replace JSHint without causing significant pain.

### Is ESLint just linting or does it also check style?

ESLint does both traditional linting (looking for problematic patterns) and style checking (enforcement of conventions). You can use it for both.

### Who is using ESLint?

The following projects are using ESLint to validate their JavaScript:

* [Drupal](https://www.drupal.org/node/2274223)
* [Esprima](https://github.com/ariya/esprima)
* [WebKit](https://bugs.webkit.org/show_bug.cgi?id=125048)

In addition, the following companies are using ESLint internally to validate their JavaScript:

* [Box](https://box.com)
* [CustomInk](https://customink.com)
* [Fitbit](http://www.fitbit.com)
* [HolidayCheck](http://holidaycheck.de)
* [the native web](http://www.thenativeweb.io)

### What about ECMAScript 6 support?

We are implementing ECMAScript 6 support piece-by-piece starting with version 0.12.0. You'll be able to opt-in to any ECMAScript 6 feature you want to use.

### Where to ask for help?

Join our [Mailing List](https://groups.google.com/group/eslint)


[npm-image]: https://img.shields.io/npm/v/eslint.svg?style=flat-square
[npm-url]: https://npmjs.org/package/eslint
[travis-image]: https://img.shields.io/travis/eslint/eslint/master.svg?style=flat-square
[travis-url]: https://travis-ci.org/eslint/eslint
[coveralls-image]: https://img.shields.io/coveralls/eslint/eslint/master.svg?style=flat-square
[coveralls-url]: https://coveralls.io/r/eslint/eslint?branch=master
[downloads-image]: http://img.shields.io/npm/dm/eslint.svg?style=flat-square
[downloads-url]: https://npmjs.org/package/eslint
