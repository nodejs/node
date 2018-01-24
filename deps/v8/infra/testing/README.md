# Src-side test specifications

The infra/testing folder in V8 contains test specifications, consumed and
executed by the continuous infrastructure. Every master has an optional file
named `<mastername>.pyl`. E.g. `tryserver.v8.pyl`.

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
The `<variant name>` is a testing variant as specified in
`v8/tools/testrunner/local/variants.py`. `<number of shards>` is optional
(default 1), but can be provided to increase the swarming shards for
long-running tests.

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