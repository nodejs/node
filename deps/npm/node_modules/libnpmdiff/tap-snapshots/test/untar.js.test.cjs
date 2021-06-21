/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/untar.js TAP filter files > should return list of filenames 1`] = `
LICENSE
README.md
`

exports[`test/untar.js TAP filter files > should return map of filenames with valid contents 1`] = `
a/LICENSE: true
a/README.md: true
`

exports[`test/untar.js TAP filter files by exact filename > should return no filenames 1`] = `

`

exports[`test/untar.js TAP filter files by exact filename > should return no filenames 2`] = `

`

exports[`test/untar.js TAP filter files using glob expressions > should return list of filenames 1`] = `
lib/index.js
lib/utils/b.js
package-lock.json
test/index.js
`

exports[`test/untar.js TAP filter files using glob expressions > should return map of filenames with valid contents 1`] = `
a/lib/index.js: true
a/lib/utils/b.js: true
a/package-lock.json: true
a/test/index.js: true
`

exports[`test/untar.js TAP match files by end of filename > should return list of filenames 1`] = `
lib/index.js
lib/utils/b.js
test/index.js
test/utils/b.js
`

exports[`test/untar.js TAP match files by end of filename > should return map of filenames with valid contents 1`] = `
a/lib/index.js: true
a/lib/utils/b.js: true
a/test/index.js: true
a/test/utils/b.js: true
`

exports[`test/untar.js TAP match files by simple folder name > should return list of filenames 1`] = `
lib/index.js
lib/utils/b.js
`

exports[`test/untar.js TAP match files by simple folder name > should return map of filenames with valid contents 1`] = `
a/lib/index.js: true
a/lib/utils/b.js: true
`

exports[`test/untar.js TAP match files by simple folder name variation > should return list of filenames 1`] = `
test/index.js
test/utils/b.js
`

exports[`test/untar.js TAP match files by simple folder name variation > should return map of filenames with valid contents 1`] = `
a/test/index.js: true
a/test/utils/b.js: true
`

exports[`test/untar.js TAP untar package with folders > should have read contents 1`] = `
module.exports = 'b'

`

exports[`test/untar.js TAP untar package with folders > should return list of filenames 1`] = `
lib/index.js
lib/utils/b.js
package-lock.json
package.json
test/index.js
test/utils/b.js
`

exports[`test/untar.js TAP untar package with folders > should return map of filenames to its contents 1`] = `
a/lib/index.js: true
a/lib/utils/b.js: true
a/package-lock.json: true
a/package.json: true
a/test/index.js: true
a/test/utils/b.js: true
`

exports[`test/untar.js TAP untar simple package > should have read contents 1`] = `
The MIT License (MIT)

Copyright (c) Ruy Adorno (ruyadorno.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

`

exports[`test/untar.js TAP untar simple package > should return list of filenames 1`] = `
LICENSE
index.js
package.json
README.md
`

exports[`test/untar.js TAP untar simple package > should return map of filenames to its contents 1`] = `
a/LICENSE: true
a/index.js: true
a/package.json: true
a/README.md: true
`
