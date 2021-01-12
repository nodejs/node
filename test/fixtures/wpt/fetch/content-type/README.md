# `resources/content-types.json`

An array of tests. Each test has these fields:

* `contentType`: an array of values for the `Content-Type` header. A harness needs to run the test twice if there are multiple values. One time with the values concatenated with `,` followed by a space and one time with multiple `Content-Type` declarations, each on their own line with one of the values, in order.
* `encoding`: the expected encoding, null for the default.
* `mimeType`: the result of extracting a MIME type and serializing it.
* `documentContentType`: the MIME type expected to be exposed in DOM documents.

(These tests are currently somewhat geared towards browser use, but could be generalized easily enough if someone wanted to contribute tests for MIME types that would cause downloads in the browser or some such.)

# `resources/script-content-types.json`

An array of tests, surprise. Each test has these fields:

* `contentType`: see above.
* `executes`: whether the script is expected to execute.
* `encoding`: how the script is expected to be decoded.

These tests are expected to be loaded through `<script src>` and the server is expected to set `X-Content-Type-Options: nosniff`.
