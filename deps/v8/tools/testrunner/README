Test suite runner for V8, including support for distributed running.
====================================================================


Local usage instructions:
=========================

Run the main script with --help to get detailed usage instructions:

$ tools/run-tests.py --help

The interface is mostly the same as it was for the old test runner.
You'll likely want something like this:

$ tools/run-tests.py --nonetwork --arch ia32 --mode release

--nonetwork is the default on Mac and Windows. If you don't specify --arch
and/or --mode, all available values will be used and run in turn (e.g.,
omitting --mode from the above example will run ia32 in both Release and Debug
modes).


Networked usage instructions:
=============================

Networked running is only supported on Linux currently. Make sure that all
machines participating in the cluster are binary-compatible (e.g. mixing
Ubuntu Lucid and Precise doesn't work).

Setup:
------

1.) Copy tools/test-server.py to a new empty directory anywhere on your hard
    drive (preferably not inside your V8 checkout just to keep things clean).
    Please do create a copy, not just a symlink.

2.) Navigate to the new directory and let the server setup itself:

$ ./test-server.py setup

    This will install PIP and UltraJSON, create a V8 working directory, and
    generate a keypair.

3.) Swap public keys with someone who's already part of the networked cluster.

$ cp trusted/`cat data/mypubkey`.pem /where/peers/can/see/it/myname.pem
$ ./test-server.py approve /wherever/they/put/it/yourname.pem


Usage:
------

1.) Start your server:

$ ./test-server.py start

2.) (Optionally) inspect the server's status:

$ ./test-server.py status

3.) From your regular V8 working directory, run tests:

$ tool/run-tests.py --arch ia32 --mode debug

4.) (Optionally) enjoy the speeeeeeeeeeeeeeeed


Architecture overview:
======================

Code organization:
------------------

This section is written from the point of view of the tools/ directory.

./run-tests.py:
  Main script. Parses command-line options and drives the test execution
  procedure from a high level. Imports the actual implementation of all
  steps from the testrunner/ directory.

./test-server.py:
  Interface to interact with the server. Contains code to setup the server's
  working environment and can start and stop server daemon processes.
  Imports some stuff from the testrunner/server/ directory.

./testrunner/local/*:
  Implementation needed to run tests locally. Used by run-tests.py. Inspired by
  (and partly copied verbatim from) the original test.py script.

./testrunner/objects/*:
  A bunch of data container classes, used by the scripts in the various other
  directories; serializable for transmission over the network.

./testrunner/network/*:
  Equivalents and extensions of some of the functionality in ./testrunner/local/
  as required when dispatching tests to peers on the network.

./testrunner/network/network_execution.py:
  Drop-in replacement for ./testrunner/local/execution that distributes
  test jobs to network peers instead of running them locally.

./testrunner/network/endpoint.py:
  Receiving end of a network distributed job, uses the implementation
  in ./testrunner/local/execution.py for actually running the tests.

./testrunner/server/*:
  Implementation of the daemon that accepts and runs test execution jobs from
  peers on the network. Should ideally have no dependencies on any of the other
  directories, but that turned out to be impractical, so there are a few
  exceptions.

./testrunner/server/compression.py:
  Defines a wrapper around Python TCP sockets that provides JSON based
  serialization, gzip based compression, and ensures message completeness.


Networking architecture:
------------------------

The distribution stuff is designed to be a layer between deciding which tests
to run on the one side, and actually running them on the other. The frontend
that the user interacts with is the same for local and networked execution,
and the actual test execution and result gathering code is the same too.

The server daemon starts four separate servers, each listening on another port:
- "Local": Communication with a run-tests.py script running on the same host.
  The test driving script e.g. needs to ask for available peers. It then talks
  to those peers directly (one of them will be the locally running server).
- "Work": Listens for test job requests from run-tests.py scripts on the network
  (including localhost). Accepts an arbitrary number of connections at the
  same time, but only works on them in a serialized fashion.
- "Status": Used for communication with other servers on the network, e.g. for
  exchanging trusted public keys to create the transitive trust closure.
- "Discovery": Used to detect presence of other peers on the network.
  In contrast to the other three, this uses UDP (as opposed to TCP).


Give us a diagram! We love diagrams!
------------------------------------
                                     .
                         Machine A   .  Machine B
                                     .
+------------------------------+     .
|        run-tests.py          |     .
|         with flag:           |     .
|--nonetwork   --network       |     .
|   |          /    |          |     .
|   |         /     |          |     .
|   v        /      v          |     .
|BACKEND    /   distribution   |     .
+--------- / --------| \ ------+     .
          /          |  \_____________________
         /           |               .        \
        /            |               .         \
+----- v ----------- v --------+     .    +---- v -----------------------+
| LocalHandler | WorkHandler   |     .    | WorkHandler   | LocalHandler |
|              |     |         |     .    |     |         |              |
|              |     v         |     .    |     v         |              |
|              |  BACKEND      |     .    |  BACKEND      |              |
|------------- +---------------|     .    |---------------+--------------|
| Discovery    | StatusHandler <----------> StatusHandler | Discovery    |
+---- ^ -----------------------+     .    +-------------------- ^ -------+
      |                              .                          |
      +---------------------------------------------------------+

Note that the three occurrences of "BACKEND" are the same code
(testrunner/local/execution.py and its imports), but running from three
distinct directories (and on two different machines).
