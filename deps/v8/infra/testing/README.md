# Src-side test specifications

Src-side test specifications enable developers to quickly add tests running on
specific bots on V8's continuous infrastructure (CI) or tryserver. Features to
be tested must live behind runtime flags, which are mapped to named testing
variants specified [here](https://chromium.googlesource.com/v8/v8/+/master/tools/testrunner/local/variants.py).
Changes to src-side test specifications go through CQ like any other CL and
require tests added for specific trybots to pass.

The test specifications are defined in a V8-side folder called infra/testing.
Every master has an optional file named `<mastername>.pyl`. E.g.
`tryserver.v8.pyl`.

The structure of each file is:
```
{
  <buildername>: [
    {
      'name': <test-spec name>,
      'variant': <variant name>,
      'shards': <number of shards>,
    },
    ...
  ],
  ...
}
```
The `<buildername>` is a string name of the builder to execute the tests.
`<test-spec name>` is a label defining a test specification matching the
[infra-side](https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/recipe_modules/v8/testing.py#58).
The `<variant name>` is a testing variant specified
[here](https://chromium.googlesource.com/v8/v8/+/master/tools/testrunner/local/variants.py).
`<number of shards>` is optional (default 1), but can be provided to increase
the swarming shards for long-running tests.

Example:
```
{
  'v8_linux64_rel_ng_triggered': [
    {'name': 'v8testing', 'variant': 'nooptimization', 'shards': 2},
  ],
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
