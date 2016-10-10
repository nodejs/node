[![NPM version][npm-image]][npm-url]
[![build status][travis-image]][travis-url]
[![Build status][appveyor-image]][appveyor-url]
[![Test coverage][coveralls-image]][coveralls-url]
[![Downloads][downloads-image]][downloads-url]
[![Bountysource](https://www.bountysource.com/badge/tracker?tracker_id=282608)](https://www.bountysource.com/trackers/282608-eslint?utm_source=282608&utm_medium=shield&utm_campaign=TRACKER_BADGE)
[![Join the chat at https://gitter.im/eslint/eslint](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/eslint/eslint?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

# ESLint

[Website](http://eslint.org) | [Configuring](http://eslint.org/docs/user-guide/configuring) | [Rules](http://eslint.org/docs/rules/) | [Contributing](http://eslint.org/docs/developer-guide/contributing) | [Reporting Bugs](http://eslint.org/docs/developer-guide/contributing/reporting-bugs) | [Twitter](https://twitter.com/geteslint) | [Mailing List](https://groups.google.com/group/eslint)

ESLint is a tool for identifying and reporting on patterns found in ECMAScript/JavaScript code. In many ways, it is similar to JSLint and JSHint with a few exceptions:

* ESLint uses [Espree](https://github.com/eslint/espree) for JavaScript parsing.
* ESLint uses an AST to evaluate patterns in code.
* ESLint is completely pluggable, every single rule is a plugin and you can add more at runtime.

## Installation

You can install ESLint using npm:

    npm install -g eslint

## Usage

If it's your first time using ESLint, you should set up a config file using `--init`:

    eslint --init

After that, you can run ESLint on any JavaScript file:

    eslint test.js test2.js

## Configuration

After running `eslint --init`, you'll have a `.eslintrc` file in your directory. In it, you'll see some rules configured like this:

```json
{
    "rules": {
        "semi": [2, "always"],
        "quotes": [2, "double"]
    }
}
```

The names `"semi"` and `"quotes"` are the names of [rules](http://eslint.org/docs/rules) in ESLint. The number is the error level of the rule and can be one of the three values:

* `0` - turn the rule off
* `1` - turn the rule on as a warning (doesn't affect exit code)
* `2` - turn the rule on as an error (exit code will be 1)

The three error levels allow you fine-grained control over how ESLint applies rules (for more configuration options and details, see the [configuration docs](http://eslint.org/docs/user-guide/configuring)).

## Sponsors

* Development is sponsored by [Box](https://box.com)
* Site search ([eslint.org](http://eslint.org)) is sponsored by [Algolia](https://www.algolia.com)

## Team

These folks keep the project moving and are resources for help:

* Nicholas C. Zakas ([@nzakas](https://github.com/nzakas)) - project lead
* Ilya Volodin ([@ilyavolodin](https://github.com/ilyavolodin)) - reviewer
* Brandon Mills ([@btmills](https://github.com/btmills)) - reviewer
* Gyandeep Singh ([@gyandeeps](https://github.com/gyandeeps)) - reviewer
* Mathias Schreck ([@lo1tuma](https://github.com/lo1tuma)) - committer
* Jamund Ferguson ([@xjamundx](https://github.com/xjamundx)) - committer
* Ian VanSchooten ([@ianvs](https://github.com/ianvs)) - committer
* Toru Nagashima ([@mysticatea](https://github.com/mysticatea)) - committer
* Burak Yiğit Kaya ([@byk](https://github.com/byk)) - committer
* Alberto Rodríguez ([@alberto](https://github.com/alberto)) - committer

## Releases

We have scheduled releases every two weeks on Friday or Saturday.

## Filing Issues

Before filing an issue, please be sure to read the guidelines for what you're reporting:

* [Bug Report](http://eslint.org/docs/developer-guide/contributing/reporting-bugs)
* [Propose a New Rule](http://eslint.org/docs/developer-guide/contributing/new-rules)
* [Request a Change](http://eslint.org/docs/developer-guide/contributing/changes)

## Frequently Asked Questions

### Why don't you like JSHint???

I do like JSHint. And I like Anton and Rick. Neither of those were deciding factors in creating this tool. The fact is that I've had a dire need for a JavaScript tool with pluggable linting rules. I had hoped JSHint would be able to do this, however after chatting with Anton, I found that the planned plugin infrastructure wasn't going to suit my purpose.

### I'm not giving up JSHint for this!

That's not really a question, but I got it. I'm not trying to convince you that ESLint is better than JSHint. The only thing I know is that ESLint is better than JSHint for what I'm doing. In the off chance you're doing something similar, it might be better for you. Otherwise, keep using JSHint, I'm certainly not going to tell you to stop using it.

### How does ESLint performance compare to JSHint and JSCS?

ESLint is slower than JSHint, usually 2-3x slower on a single file. This is because ESLint uses Espree to construct an AST before it can evaluate your code whereas JSHint evaluates your code as it's being parsed. The speed is also based on the number of rules you enable; the more rules you enable, the slower the process.

Despite being slower, we believe that ESLint is fast enough to replace JSHint without causing significant pain.

ESLint is faster than JSCS, as ESLint uses a single-pass traversal for analysis whereas JSCS using a querying model.

If you are using both JSHint and JSCS on your files, then using just ESLint will be faster.

### Is ESLint just linting or does it also check style?

ESLint does both traditional linting (looking for problematic patterns) and style checking (enforcement of conventions). You can use it for both.

### What about ECMAScript 6 support?

ESLint has full support for ECMAScript 6. By default, this support is off. You can enable ECMAScript 6 support through [configuration](http://eslint.org/docs/user-guide/configuring).

### Does ESLint support JSX?

Yes, ESLint natively supports parsing JSX syntax (this must be enabled in [configuration](http://eslint.org/docs/user-guide/configuring).). Please note that supporting JSX syntax *is not* the same as supporting React. React applies specific semantics to JSX syntax that ESLint doesn't recognize. We recommend using [eslint-plugin-react](https://www.npmjs.com/package/eslint-plugin-react) if you are using React and want React semantics.

### What about ECMAScript 7/2016 and experimental features?

ESLint doesn't natively support experimental ECMAScript language features. You can use [babel-eslint](https://github.com/babel/babel-eslint) to use any option available in Babel.

### Where to ask for help?

Join our [Mailing List](https://groups.google.com/group/eslint) or [Chatroom](https://gitter.im/eslint/eslint)


[npm-image]: https://img.shields.io/npm/v/eslint.svg?style=flat-square
[npm-url]: https://www.npmjs.com/package/eslint
[travis-image]: https://img.shields.io/travis/eslint/eslint/master.svg?style=flat-square
[travis-url]: https://travis-ci.org/eslint/eslint
[appveyor-image]: https://ci.appveyor.com/api/projects/status/iwxmiobcvbw3b0av/branch/master?svg=true
[appveyor-url]: https://ci.appveyor.com/project/nzakas/eslint/branch/master
[coveralls-image]: https://img.shields.io/coveralls/eslint/eslint/master.svg?style=flat-square
[coveralls-url]: https://coveralls.io/r/eslint/eslint?branch=master
[downloads-image]: https://img.shields.io/npm/dm/eslint.svg?style=flat-square
[downloads-url]: https://www.npmjs.com/package/eslint
