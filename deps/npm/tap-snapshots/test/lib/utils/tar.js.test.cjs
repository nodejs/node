/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/utils/tar.js TAP should log tarball contents > must match snapshot 1`] = `


package: my-cool-pkg@1.0.0
=== Tarball Contents ===

4B   cat
4B   chai
4B   dog
114B package.json
=== Bundled Dependencies ===

bundle-dep
=== Tarball Details ===

name:          my-cool-pkg                             
version:       1.0.0                                   
filename:      my-cool-pkg-1.0.0.tgz                   
package size:  271 B
unpacked size: 126 B
shasum:        23e31c8ad422f96301c07730e61ff403b10306f1
integrity:     sha512-/Lg5tEGQv5A5y[...]gq8T9D5+Wat1A==
bundled deps:  1
bundled files: 0
own files:     5
total files:   5


`

exports[`test/lib/utils/tar.js TAP should log tarball contents of a scoped package > must match snapshot 1`] = `


package: @myscope/my-cool-pkg@1.0.0
=== Tarball Contents ===

4B   cat
4B   chai
4B   dog
123B package.json
=== Bundled Dependencies ===

bundle-dep
=== Tarball Details ===

name:          @myscope/my-cool-pkg
version:       1.0.0
filename:      myscope-my-cool-pkg-1.0.0.tgz
package size:  280 B
unpacked size: 135 B
shasum:        a4f63307f2211e8fde72cd39bc1176b4fe997b71
integrity:     sha512-b+RavF8JiErJt[...]YpwkJc8ycaabA==
bundled deps:  1                                       
bundled files: 0                                       
own files:     5                                       
total files:   5                                       


`
