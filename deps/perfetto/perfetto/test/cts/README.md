This directory contains the CTS tests for the Perfetto library.

# Background
For information about what CTS is, please go to
https://source.android.com/compatibility/cts/ where you will find information
on the purpose of CTS and how to run these tests.

# Test contents
The single GTest target (CtsPerfettoTestCases) contains the following notable
test suites:
* PerfettoCtsTest - verifies that any Android app can operate as a perfetto
  producer.
* HeapprofdCtsTest - verifies that Android apps can be heap-profiled, and that
  the manifest-based opt-in is respected.
* The contents of perfetto/test/end\_to\_end\_integrationtest.cc.

## PerfettoCtsTest
The GTest suite is both the consumer of data as well as the driver the actual
tests we wish to run. This target drives the tracing system by requesting
tracing to start/stop and ensures the data it receives is correct.  This mimics
the role of real consumers acting as the driver for tracing on device. This
suite is compiled and pushed to device and subsequently run using a shell
account which gives us permissions to access the perfetto consumer socket.

The mock producer is an Android app with a thin Java wrapping around the C++
library interfaced using JNI. The purpose of this target is to ensure that the
TraceProto received from the consumer is valid and then push some fake data.
This ensures that any arbitrary app can push data to the Perfetto socket which
can then be decoded by the GTest consumer.

## HeapprofdCtsTest
The tests cover the intersection of profiling from-startup/at-runtime, and
whether the target app is debuggable. The GTest binary handles the targets'
lifetimes, acts as a profiling consumer, and asserts the contents of the
resultant traces.
