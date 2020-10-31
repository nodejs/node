Protocol Buffers - Google's data interchange format
===================================================

Copyright 2008 Google Inc.

This directory contains conformance tests for testing completeness and
correctness of Protocol Buffers implementations.  These tests are designed
to be easy to run against any Protocol Buffers implementation.

This directory contains the tester process `conformance-test`, which
contains all of the tests themselves.  Then separate programs written
in whatever language you want to test communicate with the tester
program over a pipe.

Before running any of these tests, make sure you run `make` in the base
directory to build `protoc`, since all the tests depend on it.

    $ make

Running the tests for C++
-------------------------

To run the tests against the C++ implementation, run:

    $ cd conformance && make test_cpp

Running the tests for JavaScript (Node.js)
------------------------------------------

To run the JavaScript tests against Node.js, make sure you have "node"
on your path and then run:

    $ cd conformance && make test_nodejs

Running the tests for Ruby (MRI)
--------------------------------

To run the Ruby tests against MRI, first build the C extension:

    $ cd ruby && rake

Then run the tests like so:

    $ cd conformance && make test_ruby

Running the tests for other languages
-------------------------------------

Most of the languages in the Protobuf source tree are set up to run
conformance tests.  However some of them are more tricky to set up
properly.  See `tests.sh` in the base of the repository to see how
Travis runs the tests.

Testing other Protocol Buffer implementations
---------------------------------------------

To run these tests against a new Protocol Buffers implementation, write a
program in your language that uses the protobuf implementation you want
to test.  This program should implement the testing protocol defined in
[conformance.proto](https://github.com/protocolbuffers/protobuf/blob/master/conformance/conformance.proto).
This is designed to be as easy as possible: the C++ version is only
150 lines and is a good example for what this program should look like
(see [conformance_cpp.cc](https://github.com/protocolbuffers/protobuf/blob/master/conformance/conformance_cpp.cc)).
The program only needs to be able to read from stdin and write to stdout.

Portability
-----------

Note that the test runner currently does not work on Windows.  Patches
to fix this are welcome!  (But please get in touch first to settle on
a general implementation strategy).
