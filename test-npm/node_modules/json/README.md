`json` is a fast CLI tool for working with JSON. It is a single-file node.js
script with no external deps (other than
[node.js](https://github.com/joyent/node) itself). A quick taste:

    $ echo '{"foo":"bar"}' | json
    {
      "foo": "bar"
    }

    $ echo '{"foo":"bar"}' | json foo
    bar

    $ echo '{"fred":{"age":42}}' | json fred.age    # '.' for property access
    42

    $ echo '{"age":10}' | json -e 'this.age++'
    {
      "age": 11
    }

    # `json -ga` (g == group, a == array) for streaming mode
    $ echo '{"latency":32,"req":"POST /widgets"}
    {"latency":10,"req":"GET /ping"}
    ' | json -gac 'this.latency > 10' req
    POST /widgets

Features:

- pretty-printing JSON
- natural syntax (like JS code) for extracting particular values
- get details on JSON syntax errors (handy for config files)
- filter input JSON (see `-e` and `-c` options)
- fast stream processing
- JSON validation
- in-place file editing

See <http://trentm.com/json> for full docs and examples as a man page.

Follow <a href="https://twitter.com/intent/user?screen_name=trentmick" target="_blank">@trentmick</a>
for updates to json.


# Installation

1. Get [node](http://nodejs.org).

2. `npm install -g json`

   *Note: This used to be called 'jsontool' in the npm registry, but as of
   version 8.0.0 it has taken over the 'json' name. See [npm Package
   Name](#npm-package-name) below.*

**OR manually**:

2. Get the 'json' script and put it on your PATH somewhere (it is a single file
   with no external dependencies). For example:

        cd ~/bin
        curl -L https://github.com/trentm/json/raw/master/lib/json.js > json
        chmod 755 json

You should now have "json" on your PATH:

    $ json --version
    json 9.0.0


**WARNING for Ubuntu/Debian users:** There is a current bug in Debian stable
such that "apt-get install nodejs" installed a `nodejs` binary instead of a
`node` binary. You'll either need to create a symlink for `node`, change the
`json` command's shebang line to "#!/usr/bin/env nodejs" or use
[chrislea's PPA](https://launchpad.net/~chris-lea/+archive/node.js/) as
discussed on [issue #56](https://github.com/trentm/json/issues/56). You can also do "apt-get install nodejs-legacy" to install symlink for `node` with apt.

# Test suite

    make test

You can also limit (somewhat) which tests are run with the `TEST_ONLY` envvar,
e.g.:

    cd test && TEST_ONLY=executable nodeunit test.js

I test against node 0.4 (less so now), 0.6, 0.8, and 0.10.


# License

MIT (see the fine LICENSE.txt file).


# Module Usage

Since v1.3.1 you can use "json" as a node.js module:

    var json = require('json');

However, so far the module API isn't that useful and the CLI is the primary
focus.


# npm Package Name

Once upon a time, `json` was a different thing (see [zpoley's json-command
here](https://github.com/zpoley/json-command)), and this module was
called `jsontool` in npm. As of version 8.0.0 of this module, `npm install json`
means this tool.

If you see documentation referring to `jsontool`, it is most likely
referring to this module.


# Alternatives you might prefer

- jq: <http://stedolan.github.io/jq/>
- json:select: <http://jsonselect.org/>
- json-command: <https://github.com/zpoley/json-command>
- JSONPath: <http://goessner.net/articles/JsonPath/>, <http://code.google.com/p/jsonpath/wiki/Javascript>
- jsawk: <https://github.com/micha/jsawk>
- jshon: <http://kmkeen.com/jshon/>
- json2: <https://github.com/vi/json2>
