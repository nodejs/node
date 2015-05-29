# validate-npm-package-name

Give me a string and I'll tell you if it's a valid npm package name.

This package exports a single synchronous function that takes a string as
input and returns an object:

## Valid Names

```js
var validate = require("validate-npm-package-name")

validate("some-package")
validate("example.com")
validate("under_score")
validate("123numeric")
validate("crazy!")
validate("@npm/thingy")
validate("@jane/foo.js")
```

All of the above names are valid, so you'll get this object back:

```js
{
  validForNewPackages: true,
  validForOldPackages: true
}
```

## Invalid Names

```js
  validate(" leading-space:and:weirdchars")
```

That was never a valid package name, so you get this:

```js
{
  validForNewPackages: false,
  validForOldPackages: false,
  errors: [
    'name cannot contain leading or trailing spaces',
    'name can only contain URL-friendly characters'
  ]
}
```

## Legacy Names

In the old days of npm, package names were wild. They could have capital
letters in them. They could be really long. They could be the name of an
existing module in node core.

If you give this function a package name that **used to be valid**, you'll see
a change in the value of `validForNewPackages` property, and a warnings array
will be present:

```js
validate("cRaZY-paCkAgE-with-mixed-case-and-more-than-214-characters-----------------------------------------------------------------------------------------------------------------------------------------------------------")
```

returns:

```js
{
  validForNewPackages: false,
  validForOldPackages: true,
  warnings: [
    "name can no longer contain capital letters",
    "name can no longer contain more than 214 characters"
  ]
}
```

## Tests

```sh
npm install
npm test
```

## License

ISC
