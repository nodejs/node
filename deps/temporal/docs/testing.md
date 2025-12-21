## Testing `temporal_rs`

Temporal's primary test suite is the [Temporal `test262` test
suite][test262-temporal]. There is an open issue / PR to extract
the tests from test262 into a format that `temporal_rs` can run
against in native Rust.

In the meantime, there are two primary ways to test:

1. Map the test into a Rust version in the `test` module in
   `temporal_rs`. (Ex: [Duration's tests][duration-test-mod])

2. Implement the feature in a relevant ECMAScript interpreter / engine,
   and run the engine through the test suite.

### Testing with Boa

Testing with Boa can be completed fairly easily. First and definitely
not least, implement the feature in Boa. Boa's Temporal builtin can be
found [here][boa-temporal].

Once implemented, the builtin Temporal test suite can be run with the
command:

```bash
cargo run --bin boa_tester -- run -vv --console -s test/built-ins/Temporal
```

The `intl402` Temporal test suite can be run with:

```bash
cargo run --bin boa_tester -- run -vv --console -s test/intl402/Temporal
```

#### Specify method test suites

The test suite can be further specified by qualifying the path. In order
to run the tests for `ZonedDateTime.prototype.add`, run:

```bash
cargo run --bin boa_tester -- run -vv --console -s test/built-ins/Temporal/ZonedDateTime/prototype/add
```

#### Debugging single `test262` tests

When debugging a specific test, the test output can be viewed by running
the test in verbose mode (`-vvv`):

```bash
cargo run --bin boa_tester -- run -vvv --console -s test/built-ins/Temporal/Instant/compare/argument-string-limits.js
```

Running the above command will output whether the test failed or passed,
and the test failure output if failed.

```txt
Getting last commit on 'HEAD' branch
Loading the test suite...
Test loaded, starting...
`/home/user/Projects/boa/test262/test/built-ins/Temporal/Instant/compare/argument-string-limits.js`: starting
`/home/user/Projects/boa/test262/test/built-ins/Temporal/Instant/compare/argument-string-limits.js`: Failed
`/home/user/Projects/boa/test262/test/built-ins/Temporal/Instant/compare/argument-string-limits.js`: result text
Uncaught RangeError: Instant nanoseconds are not within a valid epoch range.
```

### A note on testing with ECMAScript implementations

Any ECMAScript implementation implementing `temporal_rs` will require
some level of glue code in order to move from the implementation to
`temporal_rs`. So when debugging, it is important to determine that the
error falls in `temporal_rs`.

[test262-temporal]:
  https://github.com/tc39/test262/tree/main/test/built-ins/Temporal
[duration-test-mod]: ./src/components/duration/tests.rs
[boa-temporal]:
  https://github.com/boa-dev/boa/tree/main/core/engine/src/builtins/temporal
