/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/utils/log-file.js TAP snapshot > must match snapshot 1`] = `
0 error no prefix
1 error prefix with prefix
2 error prefix 1 2 3
3 verbose { obj: { with: { many: [Object] } } }
4 verbose {"obj":{"with":{"many":{"props":1}}}}
5 verbose {
5 verbose   "obj": {
5 verbose     "with": {
5 verbose       "many": {
5 verbose         "props": 1
5 verbose       }
5 verbose     }
5 verbose   }
5 verbose }
6 verbose [ 'test', 'with', 'an', 'array' ]
7 verbose ["test","with","an","array"]
8 verbose [
8 verbose   "test",
8 verbose   "with",
8 verbose   "an",
8 verbose   "array"
8 verbose ]
9 verbose [ 'test', [ 'with', [ 'an', [Array] ] ] ]
10 verbose ["test",["with",["an",["array"]]]]
11 verbose [
11 verbose   "test",
11 verbose   [
11 verbose     "with",
11 verbose     [
11 verbose       "an",
11 verbose       [
11 verbose         "array"
11 verbose       ]
11 verbose     ]
11 verbose   ]
11 verbose ]
12 error pre has many errors Error: message
12 error pre     at stack trace line 0
12 error pre     at stack trace line 1
12 error pre     at stack trace line 2
12 error pre     at stack trace line 3
12 error pre     at stack trace line 4
12 error pre     at stack trace line 5
12 error pre     at stack trace line 6
12 error pre     at stack trace line 7
12 error pre     at stack trace line 8
12 error pre     at stack trace line 9 Error: message2
12 error pre     at stack trace line 0
12 error pre     at stack trace line 1
12 error pre     at stack trace line 2
12 error pre     at stack trace line 3
12 error pre     at stack trace line 4
12 error pre     at stack trace line 5
12 error pre     at stack trace line 6
12 error pre     at stack trace line 7
12 error pre     at stack trace line 8
12 error pre     at stack trace line 9
13 error nostack [Error: message]

`
