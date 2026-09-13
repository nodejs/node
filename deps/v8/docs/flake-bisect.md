---
title: 'Flake bisect'
description: 'This document explains how to bisect flaky tests.'
---
Flaky tests are reported in a separate step on the bots ([example build](https://ci.chromium.org/ui/p/v8/builders/ci/V8%20Linux64%20TSAN/38630/overview)).

Each test log provides a pre-filled command line for triggering an automated flake bisect, like:

```
Trigger flake bisect on command line:
bb add v8/try.triggered/v8_flako -p 'to_revision="deadbeef"' -p 'test_name="MyTest"' ...
```

Before triggering flake bisects for the first time, users must log in with a google.com account:

```bash
bb auth-login
```

Then execute the provided command, which returns a build URL running flake bisect ([example](https://ci.chromium.org/ui/p/v8/builders/try.triggered/v8_flako/b8836020260675019825/overview)).

If you’re in luck, bisection points you to a suspect. If not, you might want to read further…

## Detailed description

For technical details, see also the implementation [tracker bug](https://crbug.com/711249). The flake bisect approach has the same intentions as [findit](https://sites.google.com/chromium.org/cat/findit), but uses a different implementation.

### How does it work?

A bisect job has 3 phases: calibration, backwards and inwards bisection. During calibration, testing is repeated doubling the total timeout (or the number of repetitions) until enough flakes are detected in one run. Then, backwards bisection doubles the git range until a revision without flakes is found. At last, we bisect into the range of the good revision and the oldest bad one. Note, bisection doesn't produce new build products, it is purely based on builds previously created on V8's continuous infrastructure.

### Bisection fails when…

- No confidence can be reached during calibration. This is typical for one-in-a-million flakes or flaky behavior only visible when other tests run in parallel (e.g. memory hungry tests).
- The culprit is too old. Bisection bails out after a certain number of steps, or if older builds are not available anymore on the isolate server.
- The overall bisect job times out. In this case it might be possible to restart it with an older known bad revision.

## Properties for customizing flake bisect

- `extra_args`: Extra arguments passed to V8’s `run-tests.py` script.
- repetitions: Initial number of test repetitions (passed to `run-tests.py`'s `--random-seed-stress-count` option; unused if `total_timeout_sec` is used).
- `timeout_sec`: Timeout parameter passed to `run-tests.py`.
- `to_revision`: Revision known to be bad. This is where bisection will start.
- `total_timeout_sec`: Initial total timeout for one entire bisect step. During calibration, this time is doubled several times if needed. Set to 0 to disable and use the `repetitions` property instead.
- `variant`: Name of the testing variant passed to `run-tests.py`.

## Properties you won’t need to change

- `bisect_buildername`: Master name of the builder that produced the builds for bisection.
- `bisect_mastername`: Name of the builder that produced the builds for bisection.
- `build_config`: Build config passed to V8’s `run-tests.py` script (there the parameter name is `--mode`, example: `Release` or `Debug`).
- `isolated_name`: Name of the isolated file (e.g. `bot_default`, `mjsunit`).
- `swarming_dimensions`: Swarming dimensions classifying the type of bot the tests should run on. Passed as list of strings, each in the format `name:value`.
- `test_name`: Fully qualified test name passed to run-tests.py. E.g. `mjsunit/foobar`.

## Tips and tricks

### Bisecting a hanging test (e.g. dead lock)

If a failing run times out, while a pass is running very fast, it is useful to tweak the timeout_sec parameter, so that bisection is not delayed waiting for the hanging runs to time out. E.g. if the pass is usually reached in <1 second, set the timeout to something small, e.g. 5 seconds.

### Getting more confidence on a suspect

In some runs, confidence is very low. E.g. calibration is satisfied if four flakes are seen in one run. During bisection, every run with one or more flakes is counted as bad. In such cases it might be useful to restart the bisect job setting to_revision to the culprit and using a higher number of repetitions or total timeout than the original job and confirm that the same conclusion is reached again.

### Working around timeout issues

In case the overall timeout option causes builds to hang, it’s best to estimate a fitting number of repetitions and set `total_timeout_sec` to `0`.

### Test behavior depending on random seed

Rarely, a code path is only triggered with a particular random seed. In this case it might be beneficial to fix it using `extra_args`, e.g. `"extra_args": ["--random-seed=123"]`. Otherwise, the stress runner uses different random seeds throughout. Note though that a particular random seed might reproduce a problem in one revision, but not in another.
