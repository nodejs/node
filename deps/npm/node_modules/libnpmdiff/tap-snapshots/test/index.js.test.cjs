/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/index.js TAP compare two diff specs > should output expected diff 1`] = `
diff --git a/index.js b/index.js
index v1.0.0..v2.0.0 100644
--- a/index.js
+++ b/index.js
@@ -1,2 +1,2 @@
 module.exports =
-  "a1"
+  "a2"
diff --git a/package.json b/package.json
index v1.0.0..v2.0.0 100644
--- a/package.json
+++ b/package.json
@@ -1,4 +1,4 @@
 {
   "name": "a",
-  "version": "1.0.0"
+  "version": "2.0.0"
 }
`

exports[`test/index.js TAP folder in node_modules nested, absolute path > should output expected diff 1`] = `
diff --git a/package.json b/package.json
index v2.0.0..v2.0.1 100644
--- a/package.json
+++ b/package.json
@@ -1,6 +1,6 @@
 {
   "name": "b",
-  "version": "2.0.0",
+  "version": "2.0.1",
   "scripts": {
     "prepare": "node prepare.js"
   }
diff --git a/prepare.js b/prepare.js
index v2.0.0..v2.0.1 100644
--- a/prepare.js
+++ b/prepare.js
@@ -1,1 +0,0 @@
-throw new Error("ERR")
/ No newline at end of file
`

exports[`test/index.js TAP folder in node_modules nested, relative path > should output expected diff 1`] = `
diff --git a/package.json b/package.json
index v2.0.0..v2.0.1 100644
--- a/package.json
+++ b/package.json
@@ -1,6 +1,6 @@
 {
   "name": "b",
-  "version": "2.0.0",
+  "version": "2.0.1",
   "scripts": {
     "prepare": "node prepare.js"
   }
diff --git a/prepare.js b/prepare.js
index v2.0.0..v2.0.1 100644
--- a/prepare.js
+++ b/prepare.js
@@ -1,1 +0,0 @@
-throw new Error("ERR")
/ No newline at end of file
`

exports[`test/index.js TAP folder in node_modules top-level, absolute path > should output expected diff 1`] = `
diff --git a/package.json b/package.json
index v1.0.0..v1.0.1 100644
--- a/package.json
+++ b/package.json
@@ -1,6 +1,6 @@
 {
   "name": "a",
-  "version": "1.0.0",
+  "version": "1.0.1",
   "scripts": {
     "prepare": "node prepare.js"
   }
diff --git a/prepare.js b/prepare.js
index v1.0.0..v1.0.1 100644
--- a/prepare.js
+++ b/prepare.js
@@ -1,1 +0,0 @@
-throw new Error("ERR")
/ No newline at end of file
`

exports[`test/index.js TAP folder in node_modules top-level, relative path > should output expected diff 1`] = `
diff --git a/package.json b/package.json
index v1.0.0..v1.0.1 100644
--- a/package.json
+++ b/package.json
@@ -1,6 +1,6 @@
 {
   "name": "a",
-  "version": "1.0.0",
+  "version": "1.0.1",
   "scripts": {
     "prepare": "node prepare.js"
   }
diff --git a/prepare.js b/prepare.js
index v1.0.0..v1.0.1 100644
--- a/prepare.js
+++ b/prepare.js
@@ -1,1 +0,0 @@
-throw new Error("ERR")
/ No newline at end of file
`
