# Node.js Core Tests

This directory contains code and data used to test the Node.js implementation.

For a detailed guide on how to write tests in this
directory, see [the guide on writing tests](../doc/guides/writing-tests.md).

On how to run tests in this direcotry, see
[the contributing guide](../CONTRIBUTING.md#step-5-test).

## Test Directories

<table>
  <thead>
    <tr>
      <th>Directory</th>
      <th>Runs on CI</th>
      <th>Purpose</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>abort</td>
      <td>No</td>
      <td>
        Tests for when the <code>--abort-on-uncaught-exception</code>
        flag is used.
      </td>
    </tr>
    <tr>
      <td>addons</td>
      <td>Yes</td>
      <td>
        Tests for <a href="https://nodejs.org/api/addons.html">addon</a>
        functionality along with some tests that require an addon to function
        properly.
      </td>
    </tr>
    <tr>
      <td>cctest</td>
      <td>Yes</td>
      <td>
        C++ test that is run as part of the build process.
      </td>
    </tr>
    <tr>
      <td>common</td>
      <td></td>
      <td>
        Common modules shared among many tests.
        <a href="./common/README.md">[Documentation]</a>
      </td>
    </tr>
    <tr>
      <td>debugger</td>
      <td>No</td>
      <td>
        Tests for <a href="https://nodejs.org/api/debugger.html">debugger</a>
        functionality along with some tests that require an addon to function
        properly.
      </td>
    </tr>
    <tr>
      <td>disabled</td>
      <td>No</td>
      <td>
        Tests that have been disabled from running for various reasons.
      </td>
    </tr>
    <tr>
      <td>fixtures</td>
      <td></td>
      <td>Test fixtures used in various tests throughout the test suite.</td>
    </tr>
    <tr>
      <td>gc</td>
      <td>No</td>
      <td>Tests for garbage collection related functionality.</td>
    </tr>
    <tr>
      <td>inspector</td>
      <td>Yes</td>
      <td>Tests for the V8 inspector integration.</td>
    </tr>
    <tr>
      <td>internet</td>
      <td>No</td>
      <td>
        Tests that make real outbound connections (mainly networking related
        modules). Tests for networking related modules may also be present in
        other directories, but those tests do not make outbound connections.
      </td>
    </tr>
    <tr>
      <td>known_issues</td>
      <td>Yes</td>
      <td>
        Tests reproducing known issues within the system. All tests inside of
        this directory are expected to fail consistently. If a test doesn't fail
        on certain platforms, those should be skipped via `known_issues.status`.
      </td>
    </tr>
    <tr>
      <td>message</td>
      <td>Yes</td>
      <td>
        Tests for messages that are output for various conditions
        (<code>console.log</code>, error messages etc.)</td>
    </tr>
    <tr>
      <td>parallel</td>
      <td>Yes</td>
      <td>Various tests that are able to be run in parallel.</td>
    </tr>
    <tr>
      <td>pseudo-tty</td>
      <td>Yes</td>
      <td>Tests that require stdin/stdout/stderr to be a TTY.</td>
    </tr>
    <tr>
      <td>pummel</td>
      <td>No</td>
      <td>
        Various tests for various modules / system functionality operating
        under load.
      </td>
    </tr>
    <tr>
      <td>sequential</td>
      <td>Yes</td>
      <td>
        Various tests that are run sequentially.
      </td>
    </tr>
    <tr>
      <td>testpy</td>
      <td></td>
      <td>
        Test configuration utility used by various test suites.
      </td>
    </tr>
    <tr>
      <td>tick-processor</td>
      <td>No</td>
      <td>
        Tests for the V8 tick processor integration. The tests are for the
        logic in <code>lib/internal/v8_prof_processor.js</code> and
        <code>lib/internal/v8_prof_polyfill.js</code>. The tests confirm that
        the profile processor packages the correct set of scripts from V8 and
        introduces the correct platform specific logic.
      </td>
    </tr>
    <tr>
      <td>timers</td>
      <td>No</td>
      <td>
        Tests for
        <a href="https://nodejs.org/api/timers.html">timing utilities</a>
        (<code>setTimeout</code> and <code>setInterval</code>).
      </td>
    </tr>
  </tbody>
</table>
