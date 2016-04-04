# Tests

### abort

Tests for when the `--abort-on-uncaught-exception` flag is used.

| Runs on CI |
|:----------:|
| No         |

### addons

Tests for [addon](https://nodejs.org/api/addons.html) functionality along with
some tests that require an addon to function properly.


| Runs on CI |
|:----------:|
| Yes        |

### cctest

C++ test that is run as part of the build process.

| Runs on CI |
|:----------:|
| Yes        |

### debugger

Tests for [debugger](https://nodejs.org/api/debugger.html) functionality.

| Runs on CI |
|:----------:|
| No         |

### disabled

Tests that have been disabled from running for various reasons.

| Runs on CI |
|:----------:|
| No         |

### fixtures

Test fixtures used in various tests throughout the test suite.

### gc

Tests for garbage collection related functionality.

| Runs on CI |
|:----------:|
| No         |

### internet

Tests that make real outbound connections (mainly networking related modules).
Tests for networking related modules may also be present in other directories,
but those tests do not make outbound connections.

| Runs on CI |
|:----------:|
| No         |

### known_issues

Tests reproducing known issues within the system.

| Runs on CI |
|:----------:|
| No         |

### message

Tests for messages that are output for various conditions (`console.log`,
error messages etc.)

| Runs on CI |
|:----------:|
| Yes        |

### parallel

Various tests that are able to be run in parallel.

| Runs on CI |
|:----------:|
| Yes        |

### pummel

Various tests for various modules / system functionality operating under load.

| Runs on CI |
|:----------:|
| No         |

### sequential

Various tests that are run sequentially.

| Runs on CI |
|:----------:|
| Yes        |

### testpy

Test configuration utility used by various test suites.

### timers

Tests for [timing utilities](https://nodejs.org/api/timers.html) (`setTimeout`
and `setInterval`).

| Runs on CI |
|:----------:|
| No         |
