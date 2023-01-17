/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/utils/exit-handler.js TAP handles unknown error with logs and debug file > debug file contents 1`] = `
0 timing npm:load:whichnode Completed in {TIME}ms
13 timing config:load Completed in {TIME}ms
14 timing npm:load:configload Completed in {TIME}ms
15 timing npm:load:mkdirpcache Completed in {TIME}ms
16 timing npm:load:mkdirplogs Completed in {TIME}ms
17 verbose title npm
18 verbose argv "--fetch-retries" "0" "--cache" "{CWD}/cache" "--loglevel" "notice"
19 timing npm:load:setTitle Completed in {TIME}ms
21 timing npm:load:display Completed in {TIME}ms
22 verbose logfile logs-max:10 dir:{CWD}/cache/_logs/{DATE}-
23 verbose logfile {CWD}/cache/_logs/{DATE}-debug-0.log
24 timing npm:load:logFile Completed in {TIME}ms
25 timing npm:load:timers Completed in {TIME}ms
26 timing npm:load:configScope Completed in {TIME}ms
27 timing npm:load Completed in {TIME}ms
28 silly logfile done cleaning log files
29 verbose stack Error: Unknown error
30 verbose cwd {CWD}/prefix
31 verbose Foo 1.0.0
32 verbose node v1.0.0
33 verbose npm  v1.0.0
34 error code ECODE
35 error ERR SUMMARY Unknown error
36 error ERR DETAIL Unknown error
37 verbose exit 1
38 timing npm Completed in {TIME}ms
39 verbose code 1
40 error A complete log of this run can be found in:
40 error     {CWD}/cache/_logs/{DATE}-debug-0.log
`

exports[`test/lib/utils/exit-handler.js TAP handles unknown error with logs and debug file > logs 1`] = `
timing npm:load:whichnode Completed in {TIME}ms
timing config:load Completed in {TIME}ms
timing npm:load:configload Completed in {TIME}ms
timing npm:load:mkdirpcache Completed in {TIME}ms
timing npm:load:mkdirplogs Completed in {TIME}ms
verbose title npm
verbose argv "--fetch-retries" "0" "--cache" "{CWD}/cache" "--loglevel" "notice"
timing npm:load:setTitle Completed in {TIME}ms
timing npm:load:display Completed in {TIME}ms
verbose logfile logs-max:10 dir:{CWD}/cache/_logs/{DATE}-
verbose logfile {CWD}/cache/_logs/{DATE}-debug-0.log
timing npm:load:logFile Completed in {TIME}ms
timing npm:load:timers Completed in {TIME}ms
timing npm:load:configScope Completed in {TIME}ms
timing npm:load Completed in {TIME}ms
silly logfile done cleaning log files
verbose stack Error: Unknown error
verbose cwd {CWD}/prefix
verbose  Foo 1.0.0
verbose node v1.0.0
verbose npm  v1.0.0
error code ECODE
error ERR SUMMARY Unknown error
error ERR DETAIL Unknown error
verbose exit 1
timing npm Completed in {TIME}ms
verbose code 1
error  A complete log of this run can be found in:
    {CWD}/cache/_logs/{DATE}-debug-0.log
`
