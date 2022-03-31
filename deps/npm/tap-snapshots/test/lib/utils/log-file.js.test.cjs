/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/utils/log-file.js TAP snapshot > must match snapshot 1`] = `
0 verbose logfile logs-max:10 dir:{CWD}/test/lib/utils/tap-testdir-log-file-snapshot
1 silly logfile done cleaning log files
2 error no prefix
3 error prefix with prefix
4 error prefix 1 2 3
5 verbose { obj: { with: { many: [Object] } } }
6 verbose {"obj":{"with":{"many":{"props":1}}}}
7 verbose {
7 verbose   "obj": {
7 verbose     "with": {
7 verbose       "many": {
7 verbose         "props": 1
7 verbose       }
7 verbose     }
7 verbose   }
7 verbose }
8 verbose [ 'test', 'with', 'an', 'array' ]
9 verbose ["test","with","an","array"]
10 verbose [
10 verbose   "test",
10 verbose   "with",
10 verbose   "an",
10 verbose   "array"
10 verbose ]
11 verbose [ 'test', [ 'with', [ 'an', [Array] ] ] ]
12 verbose ["test",["with",["an",["array"]]]]
13 verbose [
13 verbose   "test",
13 verbose   [
13 verbose     "with",
13 verbose     [
13 verbose       "an",
13 verbose       [
13 verbose         "array"
13 verbose       ]
13 verbose     ]
13 verbose   ]
13 verbose ]
14 error pre has many errors Error: message
14 error pre     at stack trace line 0
14 error pre     at stack trace line 1
14 error pre     at stack trace line 2
14 error pre     at stack trace line 3
14 error pre     at stack trace line 4
14 error pre     at stack trace line 5
14 error pre     at stack trace line 6
14 error pre     at stack trace line 7
14 error pre     at stack trace line 8
14 error pre     at stack trace line 9 Error: message2
14 error pre     at stack trace line 0
14 error pre     at stack trace line 1
14 error pre     at stack trace line 2
14 error pre     at stack trace line 3
14 error pre     at stack trace line 4
14 error pre     at stack trace line 5
14 error pre     at stack trace line 6
14 error pre     at stack trace line 7
14 error pre     at stack trace line 8
14 error pre     at stack trace line 9
15 error nostack [Error: message]

`
