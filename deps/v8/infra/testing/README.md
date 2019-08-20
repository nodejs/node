# Src-side test specifications

Src-side test specifications enable developers to quickly add tests running on
specific bots on V8's continuous infrastructure (CI) or tryserver. Features to
be tested must live behind runtime flags, which are mapped to named testing
variants specified [here](https://chromium.googlesource.com/v8/v8/+/master/tools/testrunner/local/variants.py).
Changes to src-side test specifications go through CQ like any other CL and
require tests added for specific trybots to pass.

The test specifications are defined in a V8-side python-literal file
`infra/testing/builders.pyl`.

The structure of the file is:
```
{
  <buildername>: {
    'tests': [
      {
        'name': <test-spec name>,
        'suffix': <step suffix>,
        'variant': <variant name>,
        'shards': <number of shards>,
        'test_args': <list of flags>,
        'swarming_task_attrs': {...},
        'swarming_dimensions': {...},
      },
      ...
    ],
    'swarming_task_attrs': {...},
    'swarming_dimensions': {...},
  },
  ...
}
```
The `<buildername>` is a string name of the builder to execute the tests.
`<test-spec name>` is a label defining a test specification matching the
[infra-side](https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/recipe_modules/v8/testing.py#58).
The optional `suffix` will be appended to test-step names for disambiguation.
The optional `variant` is a testing variant specified
[here](https://chromium.googlesource.com/v8/v8/+/master/tools/testrunner/local/variants.py).
The optional `shards` (default 1) can be provided to increase the swarming
shards for long-running tests.
The optional `test_args` is a list of string flags that will be passed to the
V8 test driver.
The optional `swarming_task_attrs` is a dict allowing to override the defaults
for `priority`, `expiration` and `hard_timeout`.
The optional `swarming_dimensions` is a dict allowing to override the defaults
for `cpu`, `cores` and `os`.
Both `swarming_task_attrs` and `swarming_dimensions` can be defined per builder
and per test, whereas the latter takes precedence.

Example:
```
{
  'v8_linux64_rel_ng_triggered': {
    'tests': [
      {
        'name': 'v8testing',
        'suffix': 'stress',
        'variant': 'nooptimization',
        'shards': 2,
        'test_args': ['--gc-stress'],
        'swarming_dimensions': {'os': 'Ubuntu-14.4'},
      },
    ],
    'swarming_properties': {'priority': 35},
    'swarming_dimensions': {'os': 'Ubuntu'},
  },
}
```

## Guidelines

Please keep trybots and continuous bots in sync. E.g. add the same configuration
for the release and debug CI bots and the corresponding trybot (where
applicable). E.g.

```
tryserver.v8:
  v8_linux64_rel_ng_triggered
client.v8:
  V8 Linux64
  V8 Linux64 - debug
```

Please only add tests that are expected to pass, or skip failing tests via
status file for the selected testing variants only. If you want to add FYI tests
(i.e. not closing the tree and not blocking CQ) you can do so for the following
set of bots:

```
tryserver.v8:
  v8_linux64_fyi_rel_ng_triggered
client.v8:
  V8 Linux64 - fyi
  V8 Linux64 - debug - fyi
```
