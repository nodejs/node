2017-09-26, Version 2.2.1
=========================

 * Update README to show support for Node.js 8 (Richard Chamberlain)

 * Remove use of std::map to sort version strings (Richard Chamberlain)

 * Restructure node_report.cc, move functions to utilities.cc (Richard Chamberlain)

 * Fix compile and linking issues on Alpine Linux (Richard Chamberlain)


2017-05-30, Version 2.2.0
=========================

 * Additional information for libuv handles (Richard Lau)

 * node_report.cc: Fix CreateMessage call (Howard Hellyer)

 * Allow Error object to be passed to node-report (Howard Hellyer)

 * windows: fix compile time error on mktime (Howard Hellyer)

 * report: add average CPU consumption (Bidisha Pyne)

 * Fix the use of %p to format handle pointers on non-Windows platforms. (Howard Hellyer)

 * report: add word-size of the process (LAKSHMI SWETHA GOPIREDDY)


2017-03-23, Version 2.1.2
=========================

 * Report compile time and runtime glibc version (Richard Lau)

 * Improve useability of node-report demos (Richard Chamberlain)

 * Fix return code from uncaught exception handler (Richard Chamberlain)

 * Increase tap timeout for CI testing (Richard Chamberlain)

 * test: Move OS version tests to common.js (Richard Lau)

 * docs: update platform support (Richard Lau)

 * smartos: enable node-report on SmartOS (Howard Hellyer)


2017-02-22, Version 2.1.1
=========================

 * windows: fix reporting of machine name (Richard Chamberlain)

 * test: fix test-api-getreport.js (Richard Lau)

 * aix: improve readability of os version (Richard Lau)


2017-02-13, Version 2.1.0
=========================

 * Provide getReport API to return the contents of node-report. (Howard Hellyer)


2017-02-09, Version 2.0.0
=========================

 * mac: Fix compilation errors (Howard Hellyer)

 * Rename nodereport module to node-report (Richard Chamberlain)

 * Fix source directory for install target (Richard Lau)

 * aix: skip command line check for test-fatal-error (Richard Lau)

 * Add the list of library files loaded by the process to nodereport. (Howard Hellyer)

 * docs: AIX supports triggering on USR2 signal (Richard Lau)

 * Fix behaviour on exception to match node default (Richard Chamberlain)

 * Adds the command line used to start the node process to nodereport. (Howard Hellyer)

 * Opt-in hooks and signal by default (Richard Chamberlain)

 * Fix for clang warning: libstdc++ is deprecated (Richard Chamberlain)


2016-12-12, Version 1.0.7
=========================

 * Fix version reporting in NodeReport section (Richard Lau)

 * Fix fprintf calls on Windows (Richard Lau)


2016-11-18, Version 1.0.6
=========================

 * Fix test-exception.js for PPC (Richard Lau)

 * Improve README.md (Jeremiah Senkpiel)

 * Improvement to Windows version reporting (Richard Lau)

 * Convert testcases to use tap (Richard Lau)


2016-11-10, Version 1.0.5
=========================

 * Fix for failure in fatal error (OOM) test (Richard Chamberlain)

 * Add support for nodereport on AIX (Richard Chamberlain)

 * Deleting AUTHORS file. (Richard Chamberlain)

 * Correct Javascript to JavaScript in README.md (Richard Chamberlain)

 * Correct upper-case NPM, should be lower-case (Richard Chamberlain)

 * Remove specific URLs for NPM (Richard Chamberlain)

 * Add MAINTAINER.md file to document NPM release procedure (Richard Chamberlain)

 * Set/correct package metadata in package.json (Richard Chamberlain)

 * README documentation improvements (Richard Chamberlain)

 * .gitignore the test autorun log file (Sam Roberts)

 * test: require this module using correct syntax (Sam Roberts)

 * .npmignore: do not pack unnecessary files (Sam Roberts)

 * .gitignore: ignore npm ephemera and node reports (Sam Roberts)

 * Need to define __STDC_FORMAT_MACROS for some glibc versions (Richard Chamberlain)


2016-10-28, Version 1.0.4
=========================

 * First release!
