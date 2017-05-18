## To publish an N-API version of your package alongside your non-N-API version
The following steps are illustrated using the package `iotivity-node`:
  - First, publish the non-N-API version:
    - Update the version in your `package.json`. For `iotivity-node`, the version becomes `1.2.0-2`.
    - Go through your release checklist (make sure tests, demos, and docs are OK)
    - `npm publish`
  - Then, publish the N-API version:
    - Update the version in your `package.json`. For `iotivity-node`, the version becomes `1.2.0-3`. You may place something else after the dash (-) in the version number, because that field allows quite a bit of freedom. For example, you could say `1.2.0-napi`.
    - Go through your release checklist (make sure tests, demos, and docs are OK)
    - `npm publish --tag n-api`

Tagging the release with `n-api` has ensured for `iotivity-node` that, although version 1.2.0-3 is later than the non-N-API published version (1.2.0-2), it will not be installed if someone chooses to install `iotivity-node` by simply running `npm install iotivity-node`. Thus, the non-N-API version will be installed by default. The user would have to run `npm install iotivity-node@n-api` to receive the N-API version.

## To introduce a dependency on an N-API version of a package
For example, to add the N-API version of `iotivity-node` to your package's dependencies, simply run `npm install --save iotivity-node@n-api`. Thus, you will receive version 1.2.0-3, which is the latest N-API-enabled version of iotivity-node. The dependency in your `package.json` will look like this:
```JSON
  "dependencies": {
    "iotivity-node": "n-api"
  }
```

Note that, unlike regular versions, tagged versions cannot be addressed by version ranges such as `"^2.0.0"` in your `package.json`. The reason for this is that the tag refers to exactly one version. So, if the package maintainer chooses to tag a later version of their package using the same tag, you will receive the later version when you run `npm update`. This should be acceptable given the currently experimental nature of N-API. If you wish to depend on an N-API-enabled version other than the latest published, you must change your `package.json` dependency to refer to the exact version on which you wish to depend:
```JSON
  "dependencies": {
    "iotivity-node": "1.2.0-3"
  }
```
