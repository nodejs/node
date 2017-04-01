# readdirp examples

## How to run the examples

Assuming you installed readdirp (`npm install readdirp`), you can do the following:

1. `npm explore readdirp`
2. `cd examples`
3. `npm install`

At that point you can run the examples with node, i.e., `node grep`.

## stream api

[stream-api.js](https://github.com/thlorenz/readdirp/blob/master/examples/stream-api.js)

Demonstrates error and data handling by listening to events emitted from the readdirp stream.

## stream api pipe

[stream-api-pipe.js](https://github.com/thlorenz/readdirp/blob/master/examples/stream-api-pipe.js)

Demonstrates error handling by listening to events emitted from the readdirp stream and how to pipe the data stream into
another destination stream.

## grep

[grep.js](https://github.com/thlorenz/readdirp/blob/master/examples/grep.js)

Very naive implementation of grep, for demonstration purposes only.

## using callback api

[callback-api.js](https://github.com/thlorenz/readdirp/blob/master/examples/callback-api.js)

Shows how to pass callbacks in order to handle errors and/or data.

