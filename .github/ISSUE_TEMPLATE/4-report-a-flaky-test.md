---
name: Report a flaky test
about: Report a flaky test in our CI
labels: "CI / flaky test"

---

<!--
Thank you for reporting a flaky test.

Flaky tests are tests that fail occasionally in the Node.js CI, but not
consistently enough to block PRs from landing, or that are failing in CI jobs or
test modes that are not run for every PR.

Please fill in as much of the template below as you're able.

Test: The test that is flaky - e.g. `test-fs-stat-bigint`
Platform: The platform the test is flaky on - e.g. `macos` or `linux`
Console Output: A pasted console output from a failed CI job showing the whole
failure of the test
Build Links: Links to builds affected by the flaky test

If any investigation has been done, please include any information found, such
as how consistently the test fails, whether the failure could be reproduced
locally, when the test started failing, or anything else you think is relevant.
-->

* **Test**:
* **Platform**:
* **Console Output:**
```
REPLACE ME
```
* **Build Links**:
