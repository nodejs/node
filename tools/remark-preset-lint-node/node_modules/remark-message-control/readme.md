# remark-message-control [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status] [![Chat][chat-badge]][chat]

Enable, disable, and ignore messages with [**remark**][remark].

## Installation

[npm][]:

```bash
npm install remark-message-control
```

## Usage

Say we have the following file, `example.md`:

```markdown
<!--foo ignore-->

## Heading
```

And our script, `example.js`, looks as follows:

```javascript
var vfile = require('to-vfile');
var report = require('vfile-reporter');
var remark = require('remark');
var control = require('remark-message-control');

remark()
  .use(warn)
  .use(control, {name: 'foo'})
  .process(vfile.readSync('example.md'), function (err, file) {
    console.error(report(err || file));
  });

function warn() {
  return function (tree, file) {
    var message = file.message('Whoops!', tree.children[1]);
    message.ruleId = 'thing';
    message.source = 'foo';
  };
}
```

Now, running `node example` yields:

```markdown
example.md: no issues found
```

## API

### `remark.use(control, options)`

Let comment markers control messages from a certain sources.

##### `options`

##### `options.name`

`string` — Name of markers which can control the message sources.

##### `options.known`

`Array.<string>`, optional — List of allowed `ruleId`s.  When given, a warning
is triggered when someone tries to control an unknown rule.

##### `options.reset`

`boolean`, default: `false` — Whether to treat all messages as turned off
initially.

##### `options.enable`

`Array.<string>`, optional — List of allowed `ruleId`s used when `reset: true`
to initially turn on.  By default (`reset: false`), all rules are turned on.

##### `options.disable`

`Array.<string>`, optional — List of disallowed `ruleId`s used when
`reset: false` to initially turn off.

##### `options.sources`

`string` or `Array.<string>`, optional — One or more sources which markers by
the specified `name` can control.

### Markers

#### `disable`

The “disable” marker turns off all messages of the given rule
identifiers.  When without identifiers, all messages are turned
off.

For example, to turn off certain messages:

```md
<!--lint disable list-item-bullet-indent strong-marker-->

*   **foo**

A paragraph, and now another list.

  * __bar__
```

#### `enable`

The “enable” marker turns on all messages of the given rule
identifiers.  When without identifiers, all messages are turned
on.

For example, to enable certain messages:

```md
<!--lint enable strong-marker-->

**foo** and __bar__.
```

#### `ignore`

The “ignore” marker turns off all messages of the given rule
identifiers for the duration of the following node.  When without
identifiers, all messages are turned off.

After the end of the adjacent node, messages are allowed again.

For example, to turn off certain messages for the next node:

```md
<!--lint ignore list-item-bullet-indent strong-marker-->

*   **foo**
  * __bar__
```

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/wooorm/remark-message-control.svg

[build-status]: https://travis-ci.org/wooorm/remark-message-control

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/remark-message-control.svg

[coverage-status]: https://codecov.io/github/wooorm/remark-message-control

[chat-badge]: https://img.shields.io/gitter/room/wooorm/remark.svg

[chat]: https://gitter.im/wooorm/remark

[license]: LICENSE

[author]: http://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[remark]: https://github.com/wooorm/remark
