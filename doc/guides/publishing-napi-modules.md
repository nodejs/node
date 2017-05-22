## To publish N-API version of a package alongside a non-N-API version

The following steps are illustrated using the package `iotivity-node`:
  - First, publish the non-N-API version:
    - Update the version in `package.json`. For `iotivity-node`, the version 
    becomes `1.2.0-2`.
    - Go through the release checklist (ensure tests/demos/docs are OK)
    - `npm publish`
  - Then, publish the N-API version:
    - Update the version in `package.json`. For `iotivity-node`, the version 
    becomes `1.2.0-3`. For version number, the field after the dash (-) allows 
    quite a bit of flexibility to insert any text. e.g. `1.2.0-napi`.
    - Go through the release checklist (ensure tests/demos/docs are OK)
    - `npm publish --tag n-api`

In this example, tagging the release with `n-api` has ensured that, although 
version 1.2.0-3 is later than the non-N-API published version (1.2.0-2), it 
will not be installed if someone chooses to install `iotivity-node` by simply 
running `npm install iotivity-node`. This will install the non-N-API version 
by default. The user will have to run `npm install iotivity-node@n-api` to 
receive the N-API version.

## To introduce a dependency on an N-API version of a package

To add the N-API version of `iotivity-node` as a dependency, the `package.json` 
will look like this:

```JSON
  "dependencies": {
    "iotivity-node": "n-api"
  }
```

Note that, unlike regular versions, tagged versions cannot be addressed by 
version ranges such as `"^2.0.0"` insside `package.json`. The reason for this 
is that the tag refers to exactly one version. So, if the package maintainer 
chooses to tag a later version of the package using the same tag, `npm update`
will receive the later version. This should be acceptable given the currently 
experimental nature of N-API. To depend on an N-API-enabled version other than 
the latest published, the `package.json` dependency will have to refer to the 
exact version like the following:

```JSON
  "dependencies": {
    "iotivity-node": "1.2.0-3"
  }
```
