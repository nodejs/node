/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/diff.js TAP no args in a project dir > must match snapshot 1`] = `
diff --git a/a.js b/a.js
index v0.1.0..v1.0.0 100644
--- a/a.js
+++ b/a.js
@@ -1,1 +1,1 @@
-const a = "a@0.1.0"
+const a = "a@1.0.0"
diff --git a/b.js b/b.js
index v0.1.0..v1.0.0 100644
--- a/b.js
+++ b/b.js
@@ -1,1 +1,1 @@
-const b = "b@0.1.0"
+const b = "b@1.0.0"
diff --git a/index.js b/index.js
index v0.1.0..v1.0.0 100644
--- a/index.js
+++ b/index.js
@@ -1,1 +1,1 @@
-const version = "0.1.0"
+const version = "1.0.0"
diff --git a/package.json b/package.json
index v0.1.0..v1.0.0 100644
--- a/package.json
+++ b/package.json
@@ -1,4 +1,4 @@
 {
   "name": "@npmcli/foo",
-  "version": "0.1.0"
+  "version": "1.0.0"
 }
`

exports[`test/lib/commands/diff.js TAP single arg version, filtering by files > must match snapshot 1`] = `
diff --git a/a.js b/a.js
index v0.1.0..v1.0.0 100644
--- a/a.js
+++ b/a.js
@@ -1,1 +1,1 @@
-const a = "a@0.1.0"
+const a = "a@1.0.0"
diff --git a/b.js b/b.js
index v0.1.0..v1.0.0 100644
--- a/b.js
+++ b/b.js
@@ -1,1 +1,1 @@
-const b = "b@0.1.0"
+const b = "b@1.0.0"
`

exports[`test/lib/commands/diff.js TAP various options using --name-only option > must match snapshot 1`] = `
index.js
package.json
`

exports[`test/lib/commands/diff.js TAP various options using diff option > must match snapshot 1`] = `
diff --git a/index.js b/index.js
index v2.0.0..v3.0.0 100644
--- a/index.js
+++ b/index.js
@@ -18,7 +18,7 @@
 17
 18
 19
-202.0.0
+203.0.0
 21
 22
 23
diff --git a/package.json b/package.json
index v2.0.0..v3.0.0 100644
--- a/package.json
+++ b/package.json
@@ -1,4 +1,4 @@
 {
   "name": "bar",
-  "version": "2.0.0"
+  "version": "3.0.0"
 }
`
