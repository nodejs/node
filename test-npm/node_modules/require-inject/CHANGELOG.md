## v1.4.0 (2016-06-03)

Add `requireInject.withEmptyCache` and
`requireInject.installGlobally.andClearCache` to support loading modules
to be injected with an empty cache. This can be useful when your test shares
dependencies with the module to be mocked and you need to mock a transitive
dependency of one of those dependencies. That is:

```
Test → A → B

ModuleToTest → A → MockedB
```

If we we didn't clear the cache then `ModuleToTest` would get the already
cached version of `A` and the `MockedB` would never be injected. By clearing the cache
first it means that `ModuleToTest` will get it's own copy of `A` which will then pick
up any mocks we defined.

Previously to achieve this you would need to have provided a mock for `A`,
which, if that isn't what you were testing, could be frustrating busy work.

## v1.3.1 (2016-03-04)

Properly support relative module paths.

Previously you could use them, but they would be relative to where
`require-inject` was installed.  Now they're relative to your test script. 
(I failed to notice this for so long because, through sheer coicidence, the
relative path from my own test scripts was the same as the one from
`require-inject`, but that wouldn't ordinarily be the case.)

Many, many thanks to [@jcollado](https://github.com/jcollado) who provided
the patch, with tests and was kind enough to convince me that this really
wasn't working as intended.

