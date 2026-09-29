/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/pkg.js TAP delete delete multiple field > should delete multiple fields from package.json 1`] = `
Object {
  "name": "foo",
}
`

exports[`test/lib/commands/pkg.js TAP delete delete nested field > should delete nested fields from package.json 1`] = `
Object {
  "info": Object {
    "foo": Object {
      "bar": Array [
        Object {},
      ],
    },
  },
  "name": "foo",
  "version": "1.0.0",
}
`

exports[`test/lib/commands/pkg.js TAP delete delete single field > should delete single field from package.json 1`] = `
Object {
  "name": "foo",
}
`

exports[`test/lib/commands/pkg.js TAP fix > fixes package.json issues 1`] = `
Object {
  "name": "foo",
  "version": "1.1.1",
}
`

exports[`test/lib/commands/pkg.js TAP get array field > should print retrieved array field 1`] = `
[ 'index.js', 'cli.js' ]
`

exports[`test/lib/commands/pkg.js TAP get array item > should print retrieved array field 1`] = `
index.js
`

exports[`test/lib/commands/pkg.js TAP get get array nested items notation > should print json result containing matching results 1`] = `
contributors[0].name = 'Ruy'
contributors[1].name = 'Gar'
`

exports[`test/lib/commands/pkg.js TAP get json no args > should print package.json content 1`] = `
{
  "name": "foo",
  "version": "1.1.1"
}
`

exports[`test/lib/commands/pkg.js TAP get json with args > should print package.json content 1`] = `
{
  "name": "foo"
}
`

exports[`test/lib/commands/pkg.js TAP get multiple arg > should print retrieved package.json fields 1`] = `
name = 'foo'
version = '1.1.1'
`

exports[`test/lib/commands/pkg.js TAP get multiple arg with empty value > should print retrieved package.json field regardless of empty value 1`] = `
name = 'foo'
author = ''
`

exports[`test/lib/commands/pkg.js TAP get multiple arg with only one arg existing > should print retrieved package.json field 1`] = `
name = 'foo'
`

exports[`test/lib/commands/pkg.js TAP get nested arg > node test.js 1`] = `
node test.js
`

exports[`test/lib/commands/pkg.js TAP get no args > should print package.json content 1`] = `
name = 'foo'
version = '1.1.1'
`

exports[`test/lib/commands/pkg.js TAP get non string > should print retrieved package.json field 1`] = `
{ '@npmcli/test': '*' }
`

exports[`test/lib/commands/pkg.js TAP get single arg > should print retrieved package.json field 1`] = `
1.1.1
`

exports[`test/lib/commands/pkg.js TAP set push to array syntax > should append to arrays using empty bracket syntax 1`] = `
Object {
  "keywords": Array [
    "foo",
    "bar",
    "baz",
  ],
  "name": "foo",
  "version": "1.1.1",
}
`

exports[`test/lib/commands/pkg.js TAP set set --json > should add fields to package.json 1`] = `
Object {
  "description": "awesome",
  "foo": Object {
    "bar": Object {
      "baz": "BAZ",
    },
  },
  "name": "foo",
  "private": true,
  "tap": Object {
    "timeout": 60,
  },
  "version": "1.1.1",
  "workspaces": Array [
    "packages/*",
  ],
}
`

exports[`test/lib/commands/pkg.js TAP set set = separate value > should add single field to package.json 1`] = `
Object {
  "name": "foo",
  "tap": Object {
    "test-env": Array [
      "LC_ALL=sk",
    ],
  },
  "version": "1.1.1",
}
`

exports[`test/lib/commands/pkg.js TAP set set multiple fields > should add single field to package.json 1`] = `
Object {
  "bin": Object {
    "foo": "foo.js",
  },
  "name": "foo",
  "scripts": Object {
    "test": "node test.js",
  },
  "version": "1.1.1",
}
`

exports[`test/lib/commands/pkg.js TAP set set single field > should add single field to package.json 1`] = `
Object {
  "description": "Awesome stuff",
  "name": "foo",
  "version": "1.1.1",
}
`

exports[`test/lib/commands/pkg.js TAP single workspace multiple args > should only return info for one workspace 1`] = `
a name = 'a'
a version = '1.0.0'
`

exports[`test/lib/commands/pkg.js TAP single workspace single arg > should only return info for one workspace 1`] = `
a 1.0.0
`

exports[`test/lib/commands/pkg.js TAP workspaces get > should return expected result for configured workspaces 1`] = `
a name = 'a'
a version = '1.0.0'
b name = 'b'
b version = '1.2.3'
`

exports[`test/lib/commands/pkg.js TAP workspaces get json > should return expected json result for configured workspaces 1`] = `
{
  "a": {
    "name": "a",
    "version": "1.0.0"
  },
  "b": {
    "name": "b",
    "version": "1.2.3"
  }
}
`

exports[`test/lib/commands/pkg.js TAP workspaces set > should add field to workspace a 1`] = `
Object {
  "funding": "http://example.com",
  "name": "a",
  "version": "1.0.0",
}
`

exports[`test/lib/commands/pkg.js TAP workspaces set > should add field to workspace b 1`] = `
Object {
  "funding": "http://example.com",
  "name": "b",
  "version": "1.2.3",
}
`

exports[`test/lib/commands/pkg.js TAP workspaces set > should delete version field from workspace a 1`] = `
Object {
  "funding": "http://example.com",
  "name": "a",
}
`

exports[`test/lib/commands/pkg.js TAP workspaces set > should delete version field from workspace b 1`] = `
Object {
  "funding": "http://example.com",
  "name": "b",
}
`
