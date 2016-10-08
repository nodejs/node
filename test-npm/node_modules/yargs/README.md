yargs
========

Yargs be a node.js library fer hearties tryin' ter parse optstrings.

With yargs, ye be havin' a map that leads straight to yer treasure! Treasure of course, being a simple option hash.

[![Build Status][travis-image]][travis-url]
[![Dependency Status][gemnasium-image]][gemnasium-url]
[![Coverage Status][coveralls-image]][coveralls-url]
[![NPM version][npm-image]][npm-url]
[![Windows Tests][windows-image]][windows-url]

> Yargs is the official successor to optimist. Please feel free to submit issues and pull requests. If you'd like to contribute and don't know where to start, have a look at [the issue list](https://github.com/bcoe/yargs/issues) :)

examples
========

With yargs, the options be just a hash!
-------------------------------------------------------------------

plunder.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs').argv;

if (argv.ships > 3 && argv.distance < 53.5) {
    console.log('Plunder more riffiwobbles!');
}
else {
    console.log('Retreat from the xupptumblers!');
}
````

***

    $ ./plunder.js --ships=4 --distance=22
    Plunder more riffiwobbles!

    $ ./plunder.js --ships 12 --distance 98.7
    Retreat from the xupptumblers!

![Joe was one optimistic pirate.](http://i.imgur.com/4WFGVJ9.png)

But don't walk the plank just yet! There be more! You can do short options:
-------------------------------------------------

short.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs').argv;
console.log('(%d,%d)', argv.x, argv.y);
````

***

    $ ./short.js -x 10 -y 21
    (10,21)

And booleans, both long, short, and even grouped:
----------------------------------

bool.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs').argv;

if (argv.s) {
    process.stdout.write(argv.fr ? 'Le perroquet dit: ' : 'The parrot says: ');
}
console.log(
    (argv.fr ? 'couac' : 'squawk') + (argv.p ? '!' : '')
);
````

***

    $ ./bool.js -s
    The parrot says: squawk

    $ ./bool.js -sp
    The parrot says: squawk!

    $ ./bool.js -sp --fr
    Le perroquet dit: couac!

And non-hyphenated options too! Just use `argv._`!
-------------------------------------------------

nonopt.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs').argv;
console.log('(%d,%d)', argv.x, argv.y);
console.log(argv._);
````

***

    $ ./nonopt.js -x 6.82 -y 3.35 rum
    (6.82,3.35)
    [ 'rum' ]

    $ ./nonopt.js "me hearties" -x 0.54 yo -y 1.12 ho
    (0.54,1.12)
    [ 'me hearties', 'yo', 'ho' ]

Yargs even counts your booleans!
----------------------------------------------------------------------

count.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs')
    .count('verbose')
    .alias('v', 'verbose')
    .argv;

VERBOSE_LEVEL = argv.verbose;

function WARN()  { VERBOSE_LEVEL >= 0 && console.log.apply(console, arguments); }
function INFO()  { VERBOSE_LEVEL >= 1 && console.log.apply(console, arguments); }
function DEBUG() { VERBOSE_LEVEL >= 2 && console.log.apply(console, arguments); }

WARN("Showing only important stuff");
INFO("Showing semi-important stuff too");
DEBUG("Extra chatty mode");
````

***
    $ node count.js
    Showing only important stuff

    $ node count.js -v
    Showing only important stuff
    Showing semi-important stuff too

    $ node count.js -vv
    Showing only important stuff
    Showing semi-important stuff too
    Extra chatty mode

    $ node count.js -v --verbose
    Showing only important stuff
    Showing semi-important stuff too
    Extra chatty mode

Tell users how to use yer options and make demands.
-------------------------------------------------

area.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs')
    .usage('Usage: $0 -w [num] -h [num]')
    .demand(['w','h'])
    .argv;

console.log("The area is:", argv.w * argv.h);
````

***

    $ ./area.js -w 55 -h 11
    The area is: 605

    $ node ./area.js -w 4.91 -w 2.51
    Usage: area.js -w [num] -h [num]

    Options:
      -w  [required]
      -h  [required]

    Missing required arguments: h

After yer demands have been met, demand more! Ask for non-hyphenated arguments!
-----------------------------------------

demand_count.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs')
    .demand(2)
    .argv;
console.dir(argv);
````

***

	$ ./demand_count.js a

	Not enough non-option arguments: got 1, need at least 2

	$ ./demand_count.js a b
	{ _: [ 'a', 'b' ], '$0': 'demand_count.js' }

	$ ./demand_count.js a b c
	{ _: [ 'a', 'b', 'c' ], '$0': 'demand_count.js' }

EVEN MORE SHIVER ME TIMBERS!
------------------

default_singles.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs')
    .default('x', 10)
    .default('y', 10)
    .argv
;
console.log(argv.x + argv.y);
````

***

    $ ./default_singles.js -x 5
    15

default_hash.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs')
    .default({ x : 10, y : 10 })
    .argv
;
console.log(argv.x + argv.y);
````

***

    $ ./default_hash.js -y 7
    17

And if you really want to get all descriptive about it...
---------------------------------------------------------

boolean_single.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs')
    .boolean('v')
    .argv
;
console.dir(argv.v);
console.dir(argv._);
````

***

    $ ./boolean_single.js -v "me hearties" yo ho
    true
    [ 'me hearties', 'yo', 'ho' ]


boolean_double.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs')
    .boolean(['x','y','z'])
    .argv
;
console.dir([ argv.x, argv.y, argv.z ]);
console.dir(argv._);
````

***

    $ ./boolean_double.js -x -z one two three
    [ true, false, true ]
    [ 'one', 'two', 'three' ]

Yargs is here to help you...
---------------------------

Ye can describe parameters fer help messages and set aliases. Yargs figures
out how ter format a handy help string automatically.

line_count.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs')
    .usage('Usage: $0 <command> [options]')
    .command('count', 'Count the lines in a file')
    .demand(1)
    .example('$0 count -f foo.js', 'count the lines in the given file')
    .demand('f')
    .alias('f', 'file')
    .nargs('f', 1)
    .describe('f', 'Load a file')
    .help('h')
    .alias('h', 'help')
    .epilog('copyright 2015')
    .argv;

var fs = require('fs');
var s = fs.createReadStream(argv.file);

var lines = 0;
s.on('data', function (buf) {
    lines += buf.toString().match(/\n/g).length;
});

s.on('end', function () {
    console.log(lines);
});
````

***
    $ node line_count.js count
    Usage: line_count.js <command> [options]

    Commands:
      count    Count the lines in a file

    Options:
      -f, --file  Load a file        [required]
      -h, --help  Show help           [boolean]

    Examples:
      line_count.js count -f foo.js  count the lines in the given file

    copyright 2015

    Missing required arguments: f

    $ node line_count.js count --file line_count.js
    26

    $ node line_count.js count -f line_count.js
    26

methods
=======

By itself,

````javascript
require('yargs').argv
````

will use the `process.argv` array to construct the `argv` object.

You can pass in the `process.argv` yourself:

````javascript
require('yargs')([ '-x', '1', '-y', '2' ]).argv
````

or use `.parse()` to do the same thing:

````javascript
require('yargs').parse([ '-x', '1', '-y', '2' ])
````

The rest of these methods below come in just before the terminating `.argv`.

<a name="alias"></a>.alias(key, alias)
------------------

Set key names as equivalent such that updates to a key will propagate to aliases
and vice-versa.

Optionally `.alias()` can take an object that maps keys to aliases.
Each key of this object should be the canonical version of the option, and each
value should be a string or an array of strings.

.argv
-----

Get the arguments as a plain old object.

Arguments without a corresponding flag show up in the `argv._` array.

The script name or node command is available at `argv.$0` similarly to how `$0`
works in bash or perl.

If `yargs` is executed in an environment that embeds node and there's no script name (e.g.
[Electron](http://electron.atom.io/) or [nw.js](http://nwjs.io/)), it will ignore the first parameter since it
expects it to be the script name. In order to override this behavior, use `.parse(process.argv.slice(1))`
instead of `.argv` and the first parameter won't be ignored.

<a name="array"></a>.array(key)
----------

Tell the parser to interpret `key` as an array. If `.array('foo')` is set,
`--foo foo bar` will be parsed as `['foo', 'bar']` rather than as `'foo'`.

<a name="boolean"></a>.boolean(key)
-------------

Interpret `key` as a boolean. If a non-flag option follows `key` in
`process.argv`, that string won't get set as the value of `key`.

`key` will default to `false`, unless a `default(key, undefined)` is
explicitly set.

If `key` is an array, interpret all the elements as booleans.

.check(fn)
----------

Check that certain conditions are met in the provided arguments.

`fn` is called with two arguments, the parsed `argv` hash and an array of options and their aliases.

If `fn` throws or returns a non-truthy value, show the thrown error, usage information, and
exit.

<a name="choices"></a>.choices(key, choices)
----------------------

Limit valid values for `key` to a predefined set of `choices`, given as an array
or as an individual value.

```js
var argv = require('yargs')
  .alias('i', 'ingredient')
  .describe('i', 'choose your sandwich ingredients')
  .choices('i', ['peanut-butter', 'jelly', 'banana', 'pickles'])
  .help('help')
  .argv
```

If this method is called multiple times, all enumerated values will be merged
together. Choices are generally strings or numbers, and value matching is
case-sensitive.

Optionally `.choices()` can take an object that maps multiple keys to their
choices.

Choices can also be specified as `choices` in the object given to `option()`.

```js
var argv = require('yargs')
  .option('size', {
    alias: 's',
    describe: 'choose a size',
    choices: ['xs', 's', 'm', 'l', 'xl']
  })
  .argv
```

.command(cmd, desc, [fn])
-------------------

Document the commands exposed by your application.

Use `desc` to provide a description for each command your application accepts (the
values stored in `argv._`).  Set `desc` to `false` to create a hidden command.
Hidden commands don't show up in the help output and aren't available for
completion.

Optionally, you can provide a handler `fn` which will be executed when
a given command is provided. The handler will be called with `yargs` and
`argv` as arguments.

`yargs` is a blank instance of yargs, which can be used to compose a nested
hierarchy of options handlers.

`argv` represents the arguments parsed prior to the
command being executed (those described in the outer yargs instance).

Here's an example of top-level and nested commands in action:

```js
var argv = require('yargs')
  .usage('npm <command>')
  .command('install', 'tis a mighty fine package to install')
  .command('publish', 'shiver me timbers, should you be sharing all that', function (yargs, argv) {
    argv = yargs.option('f', {
      alias: 'force',
      description: 'yar, it usually be a bad idea'
    })
    .help('help')
    .argv
  })
  .help('help')
  .argv;
```

.completion(cmd, [description], [fn]);
-------------

Enable bash-completion shortcuts for commands and options.

`cmd`: When present in `argv._`, will result in the `.bashrc` completion script
being outputted. To enable bash completions, concat the generated script to your
`.bashrc` or `.bash_profile`.

`description`: Provide a description in your usage instructions for the command
that generates bash completion scripts.

`fn`: Rather than relying on yargs' default completion functionality, which
shiver me timbers is pretty awesome, you can provide your own completion
method.

```js
var argv = require('yargs')
  .completion('completion', function(current, argv) {
    // 'current' is the current command being completed.
    // 'argv' is the parsed arguments so far.
    // simply return an array of completions.
    return [
      'foo',
      'bar'
    ];
  })
  .argv;
```

You can also provide asynchronous completions.

```js
var argv = require('yargs')
  .completion('completion', function(current, argv, done) {
    setTimeout(function() {
      done([
        'apple',
        'banana'
      ]);
    }, 500);
  })
  .argv;
```

But wait, there's more! You can return an asynchronous promise.

```js
var argv = require('yargs')
  .completion('completion', function(current, argv, done) {
    return new Promise(function (resolve, reject) {
      setTimeout(function () {
        resolve(['apple', 'banana'])
      }, 10)
    })
  })
  .argv;
```

<a name="config"></a>.config(key, [description], [parseFn])
------------

Tells the parser that if the option specified by `key` is passed in, it
should be interpreted as a path to a JSON config file. The file is loaded
and parsed, and its properties are set as arguments.

An optional `description` can be provided to customize the config (`key`) option
in the usage string.

An optional `parseFn` can be used to provide a custom parser. The parsing
function must be synchronous, and should return an object containing
key value pairs or an error.

```js
var argv = require('yargs')
  .config('settings', function (configPath) {
    return JSON.parse(fs.readFileSync(configPath, 'utf-8'))
  })
  .argv
```

<a name="count"></a>.count(key)
------------

Interpret `key` as a boolean flag, but set its parsed value to the number of
flag occurrences rather than `true` or `false`. Default value is thus `0`.

<a name="default"></a>.default(key, value, [description])
--------------------

Set `argv[key]` to `value` if no option was specified in `process.argv`.

Optionally `.default()` can take an object that maps keys to default values.

But wait, there's more! The default value can be a `function` which returns
a value. The name of the function will be used in the usage string:

```js
var argv = require('yargs')
  .default('random', function randomValue() {
    return Math.random() * 256;
  }).argv;
```

Optionally, `description` can also be provided and will take precedence over
displaying the value in the usage instructions:

```js
.default('timeout', 60000, '(one-minute)')
```

<a name="demand"></a>.demand(key, [msg | boolean])
------------------------------
.demand(count, [max], [msg])
------------------------------

If `key` is a string, show the usage information and exit if `key` wasn't
specified in `process.argv`.

If `key` is a number, demand at least as many non-option arguments, which show
up in `argv._`. A second number can also optionally be provided, which indicates
the maximum number of non-option arguments.

If `key` is an array, demand each element.

If a `msg` string is given, it will be printed when the argument is missing,
instead of the standard error message. This is especially helpful for the non-option arguments in `argv._`.

If a `boolean` value is given, it controls whether the option is demanded;
this is useful when using `.options()` to specify command line parameters.

<a name="describe"></a>.describe(key, desc)
--------------------

Describe a `key` for the generated usage information.

Optionally `.describe()` can take an object that maps keys to descriptions.

.detectLocale(boolean)
-----------

Should yargs attempt to detect the os' locale? Defaults to `true`.

.env([prefix])
--------------

Tell yargs to parse environment variables matching the given prefix and apply
them to argv as though they were command line arguments.

If this method is called with no argument or with an empty string or with `true`,
then all env vars will be applied to argv.

Program arguments are defined in this order of precedence:

1. Command line args
2. Config file
3. Env var
4. Configured defaults

```js
var argv = require('yargs')
  .env('MY_PROGRAM')
  .option('f', {
    alias: 'fruit-thing',
    default: 'apple'
  })
  .argv
console.log(argv)
```

```
$ node fruity.js
{ _: [],
  f: 'apple',
  'fruit-thing': 'apple',
  fruitThing: 'apple',
  '$0': 'fruity.js' }
```

```
$ MY_PROGRAM_FRUIT_THING=banana node fruity.js
{ _: [],
  fruitThing: 'banana',
  f: 'banana',
  'fruit-thing': 'banana',
  '$0': 'fruity.js' }
```

```
$ MY_PROGRAM_FRUIT_THING=banana node fruity.js -f cat
{ _: [],
  f: 'cat',
  'fruit-thing': 'cat',
  fruitThing: 'cat',
  '$0': 'fruity.js' }
```

Env var parsing is disabled by default, but you can also explicitly disable it
by calling `.env(false)`, e.g. if you need to undo previous configuration.

.epilog(str)
------------
.epilogue(str)
--------------

A message to print at the end of the usage instructions, e.g.

```js
var argv = require('yargs')
  .epilogue('for more information, find our manual at http://example.com');
```

.example(cmd, desc)
-------------------

Give some example invocations of your program. Inside `cmd`, the string
`$0` will get interpolated to the current script name or node command for the
present script similar to how `$0` works in bash or perl.
Examples will be printed out as part of the help message.

.exitProcess(enable)
----------------------------------

By default, yargs exits the process when the user passes a help flag, uses the
`.version` functionality, or when validation fails. Calling
`.exitProcess(false)` disables this behavior, enabling further actions after
yargs have been validated.

.fail(fn)
---------

Method to execute when a failure occurs, rather than printing the failure message.

`fn` is called with the failure message that would have been printed.

<a name="group"></a>.group(key(s), groupName)
--------------------

Given a key, or an array of keys, places options under an alternative heading
when displaying usage instructions, e.g.,

```js
var yargs = require('yargs')(['--help'])
  .help('help')
  .group('batman', 'Heroes:')
  .describe('batman', "world's greatest detective")
  .wrap(null)
  .argv
```
***
    Heroes:
      --batman  world's greatest detective

    Options:
      --help  Show help  [boolean]

.help([option, [description]])
------------------------------

Add an option (e.g. `--help`) that displays the usage string and exits the
process. If present, the `description` parameter customizes the description of
the help option in the usage string.

If invoked without parameters, `.help()` returns the generated usage string.

Example:

```js
var yargs = require("yargs")
  .usage("$0 -operand1 number -operand2 number -operation [add|subtract]");
console.log(yargs.help());
```

Later on, `argv` can be retrieved with `yargs.argv`.

.implies(x, y)
--------------

Given the key `x` is set, it is required that the key `y` is set.

Optionally `.implies()` can accept an object specifying multiple implications.

.locale()
---------

Return the locale that yargs is currently using.

By default, yargs will auto-detect the operating system's locale so that
yargs-generated help content will display in the user's language.

To override this behavior with a static locale, pass the desired locale as a
string to this method (see below).

.locale(locale)
---------------

Override the auto-detected locale from the user's operating system with a static
locale. Note that the OS locale can be modified by setting/exporting the `LC_ALL`
environment variable.

```js
var argv = require('yargs')
  .usage('./$0 - follow ye instructions true')
  .option('option', {
    alias: 'o',
    describe: "'tis a mighty fine option",
    demand: true
  })
  .command('run', "Arrr, ya best be knowin' what yer doin'")
  .example('$0 run foo', "shiver me timbers, here's an example for ye")
  .help('help')
  .wrap(70)
  .locale('pirate')
  .argv
```

***

```shell
./test.js - follow ye instructions true

Choose yer command:
  run  Arrr, ya best be knowin' what yer doin'

Options for me hearties!
  --option, -o  'tis a mighty fine option               [requi-yar-ed]
  --help        Parlay this here code of conduct             [boolean]

Ex. marks the spot:
  test.js run foo  shiver me timbers, here's an example for ye

Ye be havin' to set the followin' argument land lubber: option
```

Locales currently supported:

* **de:** German.
* **en:** American English.
* **es:** Spanish.
* **fr:** French.
* **id:** Indonesian.
* **ja:** Japanese.
* **ko:** Korean.
* **nb:** Norwegian Bokmål.
* **pirate:** American Pirate.
* **pt:** Portuguese.
* **pt_BR:** Brazilian Portuguese.
* **tr:** Turkish.
* **zh:** Chinese.

To submit a new translation for yargs:

1. use `./locales/en.json` as a starting point.
2. submit a pull request with the new locale file.

*The [Microsoft Terminology Search](http://www.microsoft.com/Language/en-US/Search.aspx) can be useful for finding the correct terminology in your locale.*

<a name="nargs"></a>.nargs(key, count)
-----------

The number of arguments that should be consumed after a key. This can be a
useful hint to prevent parsing ambiguity. For example:

```js
var argv = require('yargs')
  .nargs('token', 1)
  .parse(['--token', '-my-token']);
```

parses as:

`{ _: [], token: '-my-token', '$0': 'node test' }`

Optionally `.nargs()` can take an object of `key`/`narg` pairs.

.option(key, opt)
-----------------
.options(key, opt)
------------------

Instead of chaining together `.alias().demand().default().describe().string()`, you can specify
keys in `opt` for each of the chainable methods.

For example:

````javascript
var argv = require('yargs')
    .option('f', {
        alias: 'file',
        demand: true,
        default: '/etc/passwd',
        describe: 'x marks the spot',
        type: 'string'
    })
    .argv
;
````

is the same as

````javascript
var argv = require('yargs')
    .alias('f', 'file')
    .demand('f')
    .default('f', '/etc/passwd')
    .describe('f', 'x marks the spot')
    .string('f')
    .argv
;
````

Optionally `.options()` can take an object that maps keys to `opt` parameters.

````javascript
var argv = require('yargs')
    .options({
      'f': {
        alias: 'file',
        demand: true,
        default: '/etc/passwd',
        describe: 'x marks the spot',
        type: 'string'
      }
    })
    .argv
;
````

Valid `opt` keys include:

- `alias`: string or array of strings, alias(es) for the canonical option key, see [`alias()`](#alias)
- `array`: boolean, interpret option as an array, see [`array()`](#array)
- `boolean`: boolean, interpret option as a boolean flag, see [`boolean()`](#boolean)
- `choices`: value or array of values, limit valid option arguments to a predefined set, see [`choices()`](#choices)
- `config`: boolean, interpret option as a path to a JSON config file, see [`config()`](#config)
- `configParser`: function, provide a custom config parsing function, see [`config()`](#config)
- `count`: boolean, interpret option as a count of boolean flags, see [`count()`](#count)
- `default`: value, set a default value for the option, see [`default()`](#default)
- `defaultDescription`: string, use this description for the default value in help content, see [`default()`](#default)
- `demand`/`require`/`required`: boolean or string, demand the option be given, with optional error message, see [`demand()`](#demand)
- `desc`/`describe`/`description`: string, the option description for help content, see [`describe()`](#describe)
- `group`: string, when displaying usage instructions place the option under an alternative group heading, see [`group()`](#group)
- `nargs`: number, specify how many arguments should be consumed for the option, see [`nargs()`](#nargs)
- `requiresArg`: boolean, require the option be specified with a value, see [`requiresArg()`](#requiresArg)
- `string`: boolean, interpret option as a string, see [`string()`](#string)
- `type`: one of the following strings
    - `'array'`: synonymous for `array: true`, see [`array()`](#array)
    - `'boolean'`: synonymous for `boolean: true`, see [`boolean()`](#boolean)
    - `'count'`: synonymous for `count: true`, see [`count()`](#count)
    - `'string'`: synonymous for `string: true`, see [`string()`](#string)

.parse(args)
------------

Parse `args` instead of `process.argv`. Returns the `argv` object.

`args` may either be a pre-processed argv array, or a raw argument string.

.require(key, [msg | boolean])
------------------------------
.required(key, [msg | boolean])
------------------------------

An alias for [`demand()`](#demand). See docs there.

<a name="requiresArg"></a>.requiresArg(key)
-----------------

Specifies either a single option key (string), or an array of options that
must be followed by option values. If any option value is missing, show the
usage information and exit.

The default behavior is to set the value of any key not followed by an
option value to `true`.

.reset()
--------

Reset the argument object built up so far. This is useful for
creating nested command line interfaces.

```js
var yargs = require('yargs')
  .usage('$0 command')
  .command('hello', 'hello command')
  .command('world', 'world command')
  .demand(1, 'must provide a valid command'),
  argv = yargs.argv,
  command = argv._[0];

if (command === 'hello') {
  yargs.reset()
    .usage('$0 hello')
    .help('h')
    .example('$0 hello', 'print the hello message!')
    .argv

  console.log('hello!');
} else if (command === 'world'){
  yargs.reset()
    .usage('$0 world')
    .help('h')
    .example('$0 world', 'print the world message!')
    .argv

  console.log('world!');
} else {
  yargs.showHelp();
}
```

.showCompletionScript()
----------------------

Generate a bash completion script. Users of your application can install this
script in their `.bashrc`, and yargs will provide completion shortcuts for
commands and options.

.showHelp(consoleLevel='error')
---------------------------

Print the usage data using the [`console`](https://nodejs.org/api/console.html) function `consoleLevel` for printing.

Example:

```js
var yargs = require("yargs")
  .usage("$0 -operand1 number -operand2 number -operation [add|subtract]");
yargs.showHelp(); //prints to stderr using console.error()
```

Or, to print the usage data to `stdout` instead, you can specify the use of `console.log`:

```js
yargs.showHelp("log"); //prints to stdout using console.log()
```

Later on, `argv` can be retrieved with `yargs.argv`.

.showHelpOnFail(enable, [message])
----------------------------------

By default, yargs outputs a usage string if any error is detected. Use the
`.showHelpOnFail()` method to customize this behavior. If `enable` is `false`,
the usage string is not output. If the `message` parameter is present, this
message is output after the error message.

line_count.js:

````javascript
#!/usr/bin/env node
var argv = require('yargs')
    .usage('Count the lines in a file.\nUsage: $0 -f <file>')
    .demand('f')
    .alias('f', 'file')
    .describe('f', 'Load a file')
    .string('f')
    .showHelpOnFail(false, 'Specify --help for available options')
    .help('help')
    .argv;

// etc.
````

***

```
$ node line_count.js
Missing argument value: f

Specify --help for available options
```

.strict()
---------

Any command-line argument given that is not demanded, or does not have a
corresponding description, will be reported as an error.

<a name="string"></a>.string(key)
------------

Tell the parser logic not to interpret `key` as a number or boolean.
This can be useful if you need to preserve leading zeros in an input.

If `key` is an array, interpret all the elements as strings.

`.string('_')` will result in non-hyphenated arguments being interpreted as strings,
regardless of whether they resemble numbers.

.updateLocale(obj)
------------------
.updateStrings(obj)
------------------

Override the default strings used by yargs with the key/value
pairs provided in `obj`:

```js
var argv = require('yargs')
  .command('run', 'the run command')
  .help('help')
  .updateStrings({
    'Commands:': 'My Commands -->\n'
  })
  .wrap(null)
  .argv
```

***

```shell
My Commands -->

  run  the run command

Options:
  --help  Show help  [boolean]
```

If you explicitly specify a `locale()`, you should do so *before* calling
`updateStrings()`.

.usage(message, [opts])
---------------------

Set a usage message to show which commands to use. Inside `message`, the string
`$0` will get interpolated to the current script name or node command for the
present script similar to how `$0` works in bash or perl.

`opts` is optional and acts like calling `.options(opts)`.

.version(version, [option], [description])
----------------------------------------

Add an option (e.g. `--version`) that displays the version number (given by the
`version` parameter) and exits the process. If present, the `description`
parameter customizes the description of the version option in the usage string.

You can provide a `function` for version, rather than a string.
This is useful if you want to use the version from your package.json:

```js
var argv = require('yargs')
  .version(function() {
    return require('../package').version;
  })
  .argv;
```

.wrap(columns)
--------------

Format usage output to wrap at `columns` many columns.

By default wrap will be set to `Math.min(80, windowWidth)`. Use `.wrap(null)` to
specify no column limit (no right-align). Use `.wrap(yargs.terminalWidth())` to
maximize the width of yargs' usage instructions.

parsing tricks
==============

stop parsing
------------

Use `--` to stop parsing flags and stuff the remainder into `argv._`.

    $ node examples/reflect.js -a 1 -b 2 -- -c 3 -d 4
    { _: [ '-c', '3', '-d', '4' ],
      a: 1,
      b: 2,
      '$0': 'examples/reflect.js' }

negate fields
-------------

If you want to explicitly set a field to false instead of just leaving it
undefined or to override a default you can do `--no-key`.

    $ node examples/reflect.js -a --no-b
    { _: [], a: true, b: false, '$0': 'examples/reflect.js' }

numbers
-------

Every argument that looks like a number (`!isNaN(Number(arg))`) is converted to
one. This way you can just `net.createConnection(argv.port)` and you can add
numbers out of `argv` with `+` without having that mean concatenation,
which is super frustrating.

duplicates
----------

If you specify a flag multiple times it will get turned into an array containing
all the values in order.

    $ node examples/reflect.js -x 5 -x 8 -x 0
    { _: [], x: [ 5, 8, 0 ], '$0': 'examples/reflect.js' }

dot notation
------------

When you use dots (`.`s) in argument names, an implicit object path is assumed.
This lets you organize arguments into nested objects.

    $ node examples/reflect.js --foo.bar.baz=33 --foo.quux=5
    { _: [],
      foo: { bar: { baz: 33 }, quux: 5 },
      '$0': 'examples/reflect.js' }

short numbers
-------------

Short numeric `-n5` style arguments work too:

    $ node examples/reflect.js -n123 -m456
    { _: [], n: 123, m: 456, '$0': 'examples/reflect.js' }

installation
============

With [npm](https://github.com/npm/npm), just do:

    npm install yargs

or clone this project on github:

    git clone http://github.com/bcoe/yargs.git

To run the tests with npm, just do:

    npm test

inspired by
===========

This module is loosely inspired by Perl's
[Getopt::Casual](http://search.cpan.org/~photo/Getopt-Casual-0.13.1/Casual.pm).



[travis-url]: https://travis-ci.org/bcoe/yargs
[travis-image]: https://img.shields.io/travis/bcoe/yargs.svg
[gemnasium-url]: https://gemnasium.com/bcoe/yargs
[gemnasium-image]: https://img.shields.io/gemnasium/bcoe/yargs.svg
[coveralls-url]: https://coveralls.io/github/bcoe/yargs
[coveralls-image]: https://img.shields.io/coveralls/bcoe/yargs.svg
[npm-url]: https://www.npmjs.com/package/yargs
[npm-image]: https://img.shields.io/npm/v/yargs.svg
[windows-url]: https://ci.appveyor.com/project/bcoe/yargs
[windows-image]: https://img.shields.io/appveyor/ci/bcoe/yargs/master.svg?label=Windows%20Tests
