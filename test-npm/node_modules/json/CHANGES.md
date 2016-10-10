# json Changelog

## json 9.0.4

- [issue #108] Fix a crash on `json foo.bar` if "foo" is null.


## json 9.0.3

- [issue #82] Fix a race in `-I/--in-place` temp file creation.
  By https://github.com/inator


## json 9.0.2

- [pull #72] Correct examples in docs for conditional filtering.


## json 9.0.1

- [issue #71] Support `-o json-tab` and `-o jsony-tab` for TAB (i.e. `\t`)
  indentation of emitted JSON.


## json 9.0.0

- [issue #52] Fix termination on EPIPE in some cases.

- Add `-0`, `-2`, `-4` options to more conveniently set the JSON indentation
  without changing the mode.

- [pull #64] Add `-M, --items` option for "itemizing" key/value pairs in an
  object for easy iteration. For example:

        $ echo '{"trent":{"age":38},
                 "ewan": {"age":4}}' | json -M
        [
          {
            "key": "trent",
            "value": {
              "age": 38
            }
          },
          {
            "key": "ewan",
            "value": {
              "age": 4
            }
          }
        ]

        $ echo '{"trent":{"age":38},
                 "ewan": {"age":4}}' | json -Ma key value.age
        trent 38
        ewan 4

        # List people that can vote.
        $ echo '{"trent":{"age":38},
                 "ewan": {"age":4}}' | json -M -c 'this.value.age > 18' -a key
        trent

  Thanks to [AndrewO](https://github.com/AndrewO) for providing this!

- **Backward incompatible change to `-c CODE` and `-e CODE`** changing their
  implementation to use a JS function for processing rather than
  `vm.runInNewContext`. This is the technique for which the `-C CODE` and `-E
  CODE` options were added in version 7.0.0. Basically: This technique is
  obviously better because it is 10x faster, so it is being made the only
  supported way. `-C` and `-E`, then, become synonyms and may be removed
  in a later release.

  Unfortunately this does mean a few semantic differences in the `CODE`, the
  most noticeable of which is that **`this` is required to access the object
  fields:**

        # Bad. Works with json < v9...
        $ echo '{"green": "eggs"}' | json-v8 -e 'green="ham"'
        {
          "green": "ham"
        }

        # ... does *not* work with json v9.
        $ echo '{"green": "eggs"}' | json -e 'green="ham"'
        {
          "green": "eggs"
        }

        # Good. Works with all versions of json.
        $ echo '{"green": "eggs"}' | json -e 'this.green="ham"'
        {
          "green": "ham"
        }

  The old behaviour of `-c` and `-e` can be restored with the `JSON_EXEC=vm`
  environment variable:

        $ echo '{"green": "eggs"}' | JSON_EXEC=vm json -e 'green="ham"'
        {
          "green": "ham"
        }

  See the notes on [json 7.0.0](#json-700) below for full details on the
  performance improvements and semantic changes.


## json 8.0.0

- [pull #70] Move from 'jsontool' to 'json' in the npm registry!  Thanks
  to <https://github.com/zpoley> for graciously giving up the name, and to
  @izs for driving.  `npm install json` FTW. Here after `jsontool` will
  stagnate at version 7.0.2.


## json 7.0.2

- [issue #68] Fix `--keys, -k` handling in streaming mode, i.e. `json -gak`.


## json 7.0.1

- [pull #60, issue #59] Fix not having a `json` on the PATH from
  'npm install -g jsontool'.


## json 7.0.0

-   [issue #49] New `-C CODE` and `-E CODE` options to replace `-c CODE` and `-e
    CODE`. The new options can be **10x or more faster**. An example processing
    a large log of newline-separated JSON object as a stream:

        $ ls -al big.log
        -rw-r--r--  1 trentm  staff   156M Oct 25 23:31 big.log

        $ time json -f big.log -gac 'this.audit' req.method req.url >/dev/null

        real	0m21.380s
        user	0m21.051s
        sys	0m0.526s

        $ time json -f big.log -gaC 'this.audit' req.method req.url >/dev/null

        real	0m3.336s
        user	0m3.124s
        sys	0m0.295s

    For comparison with `jq` (a C-based fast JSON processor tool):

        $ time cat big.log | jq '.req.method, .req.url' >/dev/null

        real	0m3.307s
        user	0m3.249s
        sys	0m0.136s

    The speed difference in `json` is in how the given `CODE` is executed: the new
    implementation uses a JS function, while the `-c/-e` options use node.js's
    `vm.runInNewContext`. This change means some semantic changes to the given
    `CODE`. Some examples to show the semantic differences:

    1. `this` is required to access the object fields:

            $ echo '{"foo": "bar"}' | json -e 'foo="baz"'
            {
              "foo": "baz"
            }
            $ echo '{"foo": "bar"}' | json -e 'this.foo="baz"'
            {
              "foo": "baz"
            }

            $ echo '{"foo": "bar"}' | json -E 'foo="baz"'   # doesn't work
            {
              "foo": "bar"
            }
            $ echo '{"foo": "bar"}' | json -E 'this.foo="baz"'
            {
              "foo": "baz"
            }

    2. Explicit `return` is required with `-C` when using multiple statements:

            $ echo '{"a": 2, "b": 6}' | json -C 'sum = this.a + this.b; sum > 5'

            undefined:2
            return (sum = this.a + this.b; sum > 5)
                                         ^
            SyntaxError: Unexpected token ;

            $ echo '{"a": 2, "b": 6}' | json -C 'sum = this.a + this.b; return sum > 5'
            {
              "a": 2,
              "b": 6
            }

    3. Some operations on the input object are more as you'd expect:

            $ echo '["a", "b"]' | json -AE 'this.push("c")'
            [
              "a",
              "b",
              "c"
            ]

            $ echo '{"a":1,"b":2}' | json -E 'delete this.a'
            {
              "b": 2
            }

    4. `CODE` is no longer run in a sandbox, so you can shoot yourself in the
       foot. Security warning: *Don't run untrusted code with '-E' or '-C'.*

            $ echo '{}' | json -C 'process.stdout.end()'
            [Error: process.stdout cannot be closed.]

            $ echo '{}' | json -c 'process.stdout.end()'

            vm.js:41
                    return ns[f].apply(ns, arguments);
                                 ^
            ReferenceError: process is not defined

    Overall: (1) is a annoying, (2) is likely rare but at least is explicit,
    (3) is a minor win, (4) is something to just be aware of. The major win is a
    ~10x speed improvement!

    See <https://github.com/nfitch/node-test/blob/master/test/vmalterns.js> for
    analysis on the perf of various options for running user-given code. Thanks
    to Nate for pushing me on this!

-   Major perf win on simple lookups, and with no behaviour change(!).
    Similarly this was achieved by avoiding `vm.runInNewContext`:

        $ time json6 -f big.log -ga time >/dev/null   # v6

        real	0m28.892s
        user	0m26.839s
        sys	0m2.295s

        $ time json -f big.log -ga time >/dev/null    # v7

        real	0m2.427s
        user	0m2.289s
        sys	0m0.212s

    The changes for this have changed `jsontool.parseLookup` and
    `jsontool.handleLookup` incompatibly. However, using "jsontool.js" is
    experimental and extremely rare. Please contact me if this impacts you.

-   Support for node 0.11.x -- basically stop using `util.puts`.

    Note that apparently `vm.runInNewContext` has changed in node 0.11.x such
    that the following no longer works:

        $ echo '["a", "b"]' | json -A -e 'this[0]="A"'
        [
          "A",
          "b"
        ]

    The result with node 0.11.x is actually:

        $ echo '["a", "b"]' | json -A -e 'this[0]="A"'
        [
          "a",
          "b"
        ]

    Using the new `-E` works:

        $ echo '["a", "b"]' | json -A -E 'this[0]="A"'
        [
          "A",
          "b"
        ]

    All the more reason to use the new `-E CODE`.

- Change to 4-space indents. 'make check' clean. No functional change.

- Include project url (and my name) in `json --version`. Also show *JSON*
  formatted version info with `-j`. The point here isn't self-agrandization
  but to help differentiate from the other `json` tool out there (`npm home
  json`).

        $ json --version
        json 7.0.0
        written by Trent Mick
        https://github.com/trentm/json

        $ json --version -j
        {
          "version": "7.0.0",
          "author": "Trent Mick",
          "project": "https://github.com/trentm/json"
        }

- Validation failures with a given filename will now show the filename, e.g.:

        $ json -nf foo.json
        json: error: "foo.json" is not JSON: Expected ',' instead of '"' at line 3, column 5:
                    "baz": "car"
                ....^

- Move json.1 to "man/man1" and set "directories.man" in package.json to
  have "man json" work after "npm install -g jsontool" with the coming
  npm 1.3.3 work.


## json 6.0.0

- [Backwards incompatibility, issue #55] Drop support for grouping of adjacent
  arrays (via `-g`, `--group`) **separated by no space**:

        # Before
        echo '["one"]["two"]' | json -g
        [
          "one",
          "two"
        ]

        # After
        $ echo '["one"]["two"]' | json -g
        json: error: input is not JSON: Syntax error at line 1, column 8:
                ["one"]["two"]
                .......^
        ["one"]["two"]

  We still allow grouping of arrays separated by a newline:

        # Before and after
        $ echo '["one"]
        ["two"]' | json -g
        [
          "one",
          "two"
        ]

  This was dropped because the current regex technique used for grouping can
  fail with a JSON string that contains ']['. Not worth it.

- New `-I/--in-place` option for in-place editing of given files with:

        $ cat foo.json
        {"foo":1}

        $ json -I -f foo.json                   # format the file
        json: updated "foo.json" in-place
        $ cat foo.json
        {
          "foo": 1
        }

        $ json -I -f foo.json -e 'this.bar=42'  # add bar field
        json: updated "foo.json" in-place
        $ cat foo.json
        {
          "foo": 1,
          "bar": 42
        }

  Note: I'd have loved to have used `-i` a la sed, but that is unfortunately
  taken in this tool.


## json 5.1.3

- Fix an issue with option parsing that resulted in this failing:

        json -f foo.json -- -1.someKey



## json 5.1.2

- [pull #43] Add '-n' as a short alias for '--validate'. (By Bill Pijewski,
  github.com/pijewski).


## json 5.1.1

- [issue #42] Fix an edge case where a blank line would be emitted for
  `... | json -ga -c COND` where the `COND` resulted in no matches.
- [issue #40] Improve "Lookups" section of docs to show how to lookup
  non-identifier keys.


## json 5.1.0

- [pull #39, issue #34] `json -ga` streams. (Largely by Fred Kuo, github.com/fkuo)
  This means you can use `json` with an input stream of JSON objects. E.g.:

        yes '{"foo":"bar"}' | json -ga

  Limitations: As was already a limitation of the '-g' switch, this only
  supports JSON objects delimited by newlines or with no space:

        {"a":1}{"b":2}          # good
        {"a":1}\n{"b":2}        # good
        {"a":1} {"b":2}         # bad

  Additionally, currently only a stream of *objects* is supported, not
  a stream of arrays. Such are the typical use cases I've encountered.


## json 5.0.0

- [**backward incompatible**, issue #35] Special case the output for **a single
  lookup AND JSON output** (i.e. `-j` or `-o json*`) to only output the value
  instead of the more general array or table that is necessary for multiple
  lookups. For objects:

        # Before:
        $ echo '{"one": "un", "two": "deux", "three": "troix"}' | json -j one
        {
          "one": "un"
        }

        # After:
        $ echo '{"one": "un", "two": "deux", "three": "troix"}' | json -j one
        "un"

        # Unchanged:
        $ echo '{"one": "un", "two": "deux", "three": "troix"}' | json -j one two
        {
          "one": "un",
          "two": "deux"
        }

  For arrays:

        # Before:
        $ echo '["a", "b", "c"]' | json -j 0
        [
          "a"
        ]

        # After:
        $ echo '["a", "b", "c"]' | json -j 0
        "a"

        # Unchanged:
        $ echo '["a", "b", "c"]' | json -j 0 1
        [
          "a",
          "b"
        ]

  The motivation for this change was (a) the WTF of the first example above and
  (b) issue #36 where one could no longer extract a single value using '-j' to
  explicitly get JSON string quoting.



## json 4.0.1

- [issue #36] Turn off coloring for inspect output (`json -i`, `json -o
  inspect`) if stdout is not a TTY.


## json 4.0.0

- Add `--validate` option to just validate (no processing and output)

        $ echo '{"foo" "bar"}' | json
        json: error: input is not JSON: Expected ':' instead of '"' at line 1, column 8:
                {"foo" "bar"}
                .......^
        {"foo" "bar"}
        $ echo '{"foo" "bar"}' | json --validate
        json: error: input is not JSON: Expected ':' instead of '"' at line 1, column 8:
                {"foo" "bar"}
                .......^
        $ echo '{"foo" "bar"}' | json --validate -q
        $ echo $?
        1

- Add `-f FILE` option for specifying an input file (or files) instead of
  stdin:

        $ json -f foo.json
        {
          "foo": "bar"
        }
        $ json -f foo.json foo
        bar

- [Backward incompatible] Move "auto-arrayification" to require explicitly
  using the "-g" or "--group" option:

        $ echo '{"one":"two"}{"three":"four"}' | json
        json: error: input is not JSON: Syntax error at line 1, column 14:
                {"one":"two"}{"three":"four"}
                .............^
        {"one":"two"}{"three":"four"}
        $ echo '{"one":"two"}{"three":"four"}' | json -g -o json-0
        [{"one":"two"},{"three":"four"}]

  This is to avoid auto-arrayification accidentally making an invalid JSON
  object (with a missing comma) be transformed to a valid array:

        $ cat oops.json
        {
          "a": {
            "b": [
                {"foo": "bar"}
                {"foo": "bar"}
            ]
          }
        }
        $ cat oops.json | json3 -o json-0
        [{"a":{"b":[{"foo":"bar"},{"foo":"bar"}]}}]

  Basically the jusitification for this breaking change is that the
  invariant of `json` validating the input JSON is more important than the
  occassional convenience of grouping.

- Use 8 space indent for syntax error message on stderr instead of '\t'.
  Minor change. Tabs are evil.


## json 3.3.0

- Add `-k|--keys` option to output the input objects keys:

        $ echo '{"name": "trent", "age": 38}' | json -k
        [
          "name",
          "age"
        ]
        $ echo '{"name": "trent", "age": 38}' | json -ka
        name
        age

- Drop jsontool v2 dependency. This had been added for the first few json3
  releases to provide a `json2` for comparison. `json` v3 is fairing well
  enough now to not bother.


## json 3.2.0

- Support negative array indeces (a la Python list indeces), e.g.:

        $ echo '["a", "b", "c"]' | json -- -1
        c


## json 3.1.2

- Update man page and move bulk examples from README to man page. Use ronn (the
  ruby one) instead of ronnjs: better and more reliable formatting. Add 'make
  docs' and 'make publish' (the latter to push to GH pages at
  <http://trentm.com/json>).
- [issue #31] Fix error message for `json -o`.


## json 3.1.1

- [issue #32] Fix '-D' option processing so `json -D/` works (no space).


## json 3.1.0

- [pull #29] Add '-D' option to set a delimiter for lookups (default is '.'),
  so that this example works:

      $ echo '{"a.b": {"b": 1}}' | json -D / a.b/b
      1

  By Yaniv Aknin.


## json 3.0.3

- [issue #30] Fix lookup strings with multiple double-quotes.
- [issue #28] Don't error on a multi-level lookup where one of the components
  is undefined. E.g., the following is no longer an error:

        $ echo '{"foo": "bar"}' | json not_foo.bar


## json 3.0.2

- [issue #27] Fix issue handling multi-level lookups (e.g. 'json foo.bar').


## json 3.0.1

- Fix a bogus 'json' dep.


## json 3.0.0

- Switched to json 3.x dev on master. "2.x" branch created for any
  necessary 2.x releases. See the
  [2.x changelog here](https://github.com/trentm/json/blob/2.x/CHANGES.md).

- [Backward incompatible] A significant change to 'jsony' default output and
  some use cases to increase utility. These changes necessitated a few
  backward incompatible changes. However, care was take to only break
  compat for (a) rare use cases and (b) where utility was much improved.
  See <https://github.com/trentm/json/wiki/backward-incompat-json-3-changes>
  for full details of backward incompatible changes.

- New "conditional filtering" via the `-c CODE` option. If the input is an
  array, then `-c` will automatically switch to processing each element
  of the array. Example:

        $ echo '[{"name":"trent", "age":38}, {"name":"ewan", "age":4}]' \
            | json3 -c 'age>21'
        [
          {
            "name": "trent",
            "age": 38
          }
        ]

- Change `-e CODE` option to automatically switch to array processing if
  the input is an array. This matches the behaviour of the new `-c CODE`
  option. Example:

        $ echo '[{"name":"trent", "age":38}, {"name":"ewan", "age":4}]' \
            | json3 -e 'age++' -o json-0
        [{"name":"trent","age":39},{"name":"ewan","age":5}]

- New '-A' option to force `-e` and `-c` to process an input array as a
  single item.

- Add [ansidiff](https://github.com/trentm/ansidiff)-based colored diffs
  for test suite failures.

- [issue #26] Add support for escapes in the delimiter given by `-d DELIM`:

        $ echo '[{"one":"un","two":"deux"},{"one":"uno","two":"dos"}]' \
            | json -a -d'\t' one two
        un	deux
        uno	dos


## json 2.2.1

- Hack workaround for issue #24 to not get a spurious "process.stdout cannot be
  closed" from current node 0.6 versions. Note: currently this guard is only
  applied for node v0.6.0..v0.6.8 inclusive.


## json 2.2.0

- New "-e CODE" option to execute the given code on the input object; or,
  if '-a/--array' is given, then on each item in the input array. Execution
  is done before filtering.

        $ echo '{"age": 38}' | json -e 'this.age++'
        {
          "age": 39
        }


## json 2.1.0

- Improve error message when input is not JSON to include context and line
  and column position. This is implemented using a JSON parser from
  (<https://github.com/douglascrockford/JSON-js>). Example:

        $ echo "[1,,2]" | json
        json: error: input is not JSON: Unexpected ',' at line 1, column 4:
            [1,,2]
            ...^
        [1,,2]


## json 2.0.3

- Auto-arrayification: Drop support for arrayifying an array adjacent to
  an object. I.e. only arrayify adjacent objects *or* adjacent arrays.

- Auto-arrayification: Change "arrayification" of adjacent *arrays* to be
  a single flat arrays of the input arrays' elements. Before:

        $ echo '[1,2][3,4]' | bin/json
        [
          [
            1,
            2,
          ],
          [
            3,
            4
          ]
        ]

    and now:

        $ echo '[1,2][3,4]' | bin/json
        [
          1,
          2,
          3,
          4
        ]

    This is expected to be more useful in practice.

-   Auto-arrayification: Allow JSON objects (or arrays) to be "arrayified"
    if not separated by any space. Previously a newline (at least) separation
    was required. So, for example, the following now works:

        $ echo '{"a":1}{"b":2}' | bin/json -o json-0
        [{"a":1},{"b":2}]

    The rules for auto-arrayification then are: Objects and arrays only,
    separated by no space or space including a newline.

- Fix stdout flushing in some cases.


## json 2.0.2

- Add node v0.6 support. Drop v0.2 and v0.3 support.


## json 2.0.1

- [issue#23] Fix output in '-a|--array' mode if one or more keys don't
  exist in one or more of the array items.


## json 2.0.0

-   '-o | --output MODE' support. Supported modes:

        jsony (default): JSON with string quotes elided
        json: JSON output, 2-space indent
        json-N: JSON output, N-space indent, e.g. 'json-4'
        inspect: node.js `util.inspect` output

-   '-a|--array' for independently processing each element of an input array.

        $ echo '[
        {
          "name": "Trent",
          "id": 12,
          "email": "trent@example.com"
        },
        {
          "name": "Mark",
          "id": 13,
          "email": "mark@example.com"
        }
        ]' | json -a name email
        Trent trent@example.com
        Mark mark@example.com

    This example shows that '-a' results in tabular output. The '-d' option
    can be used to specify a delimiter other than the default single space, e.g.:

        json -d, -a field1 field2

    [Backward Incompatibility] This is a replacement for the experimental '*'
    syntax in the lookup strings (previously enabled via '-x|--experimental').
    That syntax and option has been removed.

-   Add '--' option processing support and error out if an unknown option is
    given.

-   Support multiple top-level JSON objects as input to mean a list of
    these object:

        $ echo '{"one": 1}
        {"two": 1}' | ./lib/jsontool.js
        [
          {
            "one": 1
          },
          {
            "two": 1
          }
        ]

    This can be nice to process a stream of JSON objects generated from
    multiple calls to another tool or `cat *.json | json`. Rules:

    -   Only JS objects and arrays. Don't see strong need for basic
        JS types right now and this limitation simplifies.
    -   The break between JS objects has to include a newline. I.e. good:

            {"one": 1}
            {"two": 2}

        bad:

            {"one": 1}{"two": 2}

        This condition should be fine for typical use cases and ensures
        no false matches inside JS strings.


## json 1.4.1

- [issue #9] Gracefully handle EPIPE (i.e. stdout being closed on json before
  it is finished writing).


## json 1.4.0

- [issue #19] Allow multiple lookup arguments:

        $ echo '{"one": 1, "two": 2}' | json one two
        1
        2

  WARNING: This involve a backward incompatible change in the JS APIs
  `jsontool.processDatum` and `jsontool.processDatumExperimental`.


## json 1.3.4

- [issue #18] Fix `json --version` for standalone mode again (was broken in json 1.3.3).


## json 1.3.3

- WARNING: `json --version` is broken when running outside the source (or npm
  install'd) tree. I.e. this is a bad release for standalone.
- [issue #17] Ensure stdout is flushed on exit.


## json 1.3.2

- [issue #16] Fix to use `<regex object>.exec` instead of using the regex
  object as a function -- no longer allowed in the v8 used in node v0.5.x.


## json 1.3.1

- Make "jsontool" require'able as a module. For example, you can now:

        $ npm install jsontool
        $ node
        > var jsontool = require('jsontool')
        > jsontool.parseLookup('a.b.c')
        [ 'a', 'b', 'c' ]
        > jsontool.parseLookup('my-key.0["bar"]')
        [ 'my-key', '0', '["bar"]' ]
        > jsontool.main(['', '', '--help'])
        Usage: <something generating JSON on stdout> | json [options] [lookup]
        ...

  Currently other exported API is experimental and will likely change to be
  more generally useful (e.g. the current `processDatum` isn't all handy
  for module usage).

  Note: For command-line usage, the main module has moved from "json" to
  "lib/jsontool.js". So, if you are not using npm, you can setup the `json`
  command via something like:

        alias json='.../json/lib/jsontool.js'


## json 1.3.0

- package.json and publish to npm as "jsontool" ("json" name is taken)
- Add experimental support for '*' in the lookup. This will extract all
  the elements of an array. Examples:

        $ echo '["a", "b", "c"]' | json -x '*'
        a
        b
        c
        $ echo '[{"one": "un"}, {"two": "deux"}]' | json -x '*'
        {
          "one": "un"
        }
        {
          "two": "deux"
        }
        $ echo '[{"foo": "bar"}, {"foo": "baz"}]' | json -x '*.foo'
        bar
        baz

  This is still experimental because I want to feel it out (is it useful?
  does it cause problems for regular usage?) and it is incomplete. The
  second example above shows that with '\*', json can emit multiple JSON
  documents. `json` needs to change to support *accepting* multiple JSON
  documents.

  Also, a limitation: How to extract *multiple* fields from a list of
  objects? Is this even a necessary feature? Thinking out loud:

        '*.{name,version}'      # a la bash. Josh likes it. What else do you need?

- Add '-x|--experimental' option to turn on incomplete/experimental features.


## json 1.2.1

- [issue #12] Fix handling of output when result of lookup is `undefined`.


## json 1.2.0

- [issue #10] Fix for node v0.5.


## json 1.1.9

- [Issue 8] Don't emit a newline for empty output.


## json 1.1.8

- [Issue 7] Handle "HTTP/1.1 100 Continue" leading header block.
- [Issue 4] Add a man page (using ronnjs).


## json 1.1.7

- [Issue 5] Fix getting a key with a period. E.g.:

        echo '{"foo.bar": 42}' | json '["foo.bar"]'

  `json` is now doing much better lookup string parsing. Because escapes are
  now handled properly you can do the equivalent a little more easily:

        $ echo '{"foo.bar": 42}' | json foo\\.bar
        42


## json 1.1.6

- [Issue 6] Error exit value if invalid JSON.


## json 1.1.4

- [Issue 2] Fix bracket notation: `echo '{"foo-bar": "baz"}' | json '["foo-bar"]'`


(Started maintaining this log 19 March 2011. For earlier change information
you'll have to dig into the commit history.)
