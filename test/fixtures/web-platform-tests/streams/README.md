# Streams Tests

The work on the streams tests is closely tracked by the specification authors, who maintain a reference implementation intended to match the spec line-by-line while passing all of these tests. See [the whatwg/streams repository for details](https://github.com/whatwg/streams/tree/master/reference-implementation). Some tests may be in that repository while the spec sections they test are still undergoing heavy churn.

## Generating wrapper files

Because the streams feature is supposed to work in all global contexts, each test is written as a `.js` file, and then four `.html` files are generated around it. So for example, for `count-queueing-strategy.js`, we have the wrapper files:

- `count-queueing-strategy.https.html`
- `count-queueing-strategy.dedicatedworker.html`
- `count-queueing-strategy-sharedworker.html`
- `count-queueing-strategy-serviceworker.html`

These are generated automatically by the Node.js script in `generate-test-wrappers.js`. See it for details, and please remember to use it whenever adding new tests.
