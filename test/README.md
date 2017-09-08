# Node.js Core Tests

This directory contains code and data used to test the Node.js implementation.

For a detailed guide on how to write tests in this
directory, see [the guide on writing tests](../doc/guides/writing-tests.md).

On how to run tests in this directory, see
[the contributing guide](../CONTRIBUTING.md#step-5-test).

## Test Directories

|Directory          |Runs on CI     |Purpose        |
|-------------------|---------------|---------------|
|abort              |No             |Tests for when the  ``` --abort-on-uncaught-exception ``` flag is used.|
|addons             |Yes            |Tests for [addon](https://nodejs.org/api/addons.html) functionality along with some tests that require an addon to function  properly.|
|cctest             |Yes            |C++ test that is run as part of the build process.|
|common             |               |Common modules shared among many tests. [Documentation](./common/README.md)|
|debugger           |No             |Tests for [debugger](https://nodejs.org/api/debugger.html) functionality along with some tests that require an addon to function properly.|
|fixtures           |               |Test fixtures used in various tests throughout the test suite.|
|gc                 |No             |Tests for garbage collection related functionality.|
|inspector          |Yes            |Tests for the V8 inspector integration.|
|internet           |No             |Tests that make real outbound connections (mainly networking related modules). Tests for networking related modules may also be present in        other directories, but those tests do not make outbound connections.|
|known_issues       |Yes            |Tests reproducing known issues within the system. All tests inside of this directory are expected to fail consistently. If a test doesn't fail on certain platforms, those should be skipped via `known_issues.status`.|
|message            |Yes            |Tests for messages that are output for various conditions (```console.log```, error messages etc.)|
|parallel           |Yes            |Various tests that are able to be run in parallel.|
|pseudo-tty         |Yes            |Tests that require stdin/stdout/stderr to be a TTY.|
|pummel             |No             |Various tests for various modules / system functionality operating under load.|
|sequential         |Yes            |Various tests that are run sequentially.|
|testpy             |               |Test configuration utility used by various test suites.|
|tick-processor     |No             |Tests for the V8 tick processor integration. The tests are for the logic in ```lib/internal/v8_prof_processor.js``` and  ```lib/internal/v8_prof_polyfill.js```. The tests confirm that the profile processor packages the correct set of scripts from V8 and introduces the correct platform specific logic.|
|timers             |No             |Tests for [timing utilities](https://nodejs.org/api/timers.html) (```setTimeout``` and ```setInterval```).|
