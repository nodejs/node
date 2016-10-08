require-inject
--------------

A simple mock injector compatible needing no instrumentation in the libraries being tested

### Example

    var requireInject = require('require-inject');

    var mymod = requireInject('mymod', {
        'fs' => {
            stat: function (file,cb) {
                switch (file) {
                case 'testfile1': return cb(null,{})
                case 'testfile2': return cb(new Error('ENOENT'))
                }
            }
        }
    })

    var myglobal = requireInject.installGlobally('myglobal', { … })

### Usage in your tests

* **`var mymod = requireInject( module, mocks )`**

*module* is the name of the module you want to require.  This is what you'd
pass to `require` to load the module from your script. This means that for
relative paths, the path should be relative to your test script, not to the
thing you're injecting dependencies into.

*mocks* is an object with keys that are the names of the modules you want
to mock and values of the mock version of the objects.

**requireInject** makes it so that when *module* is required, any of its
calls to require for modules inclued in *mocks* will return the mocked
version.  It takes care to not impact any other uses of *module*, any
calls to require for it will get a version without mocks.

* **`var mymod = requireInject.withClearCache(module, mocks)`**

As with `requireInject` but your require cache will be cleared before requring
the module to have mocks injected into it. This can be useful when your test shares
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

* **`var myglobal = requireInject.installGlobally( module, mocks)`**

As with `requireInject`, except that the module and its mocks are left in
the require cache and any future requires will end up using them too.  This
is helpful particularly in the case of things that defer loading (that is,
do async loading).

* **`var myglobal = requireInject.installGlobally.andClearCache(module, mocks)`**

As with `requireInject.installGlobally` but clear the cache first as with
`requireInject.withClearCache`.  Because this globally clears the cache it
means that any requires after this point will get fresh copies of their
required modules, even if you required them previously.
