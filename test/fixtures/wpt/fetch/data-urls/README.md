## data: URLs

`resources/data-urls.json` contains `data:` URL tests. The tests are encoded as a JSON array. Each value in the array is an array of two or three values. The first value describes the input, the second value describes the expected MIME type, null if the input is expected to fail somehow, or the empty string if the expected value is `text/plain;charset=US-ASCII`. The third value, if present, describes the expected body as an array of integers representing bytes.

These tests are used for `data:` URLs in this directory (see `processing.any.js`).

## Forgiving-base64 decode

`resources/base64.json` contains [forgiving-base64 decode](https://infra.spec.whatwg.org/#forgiving-base64-decode) tests. The tests are encoded as a JSON array. Each value in the array is an array of two values. The first value describes the input, the second value describes the output as an array of integers representing bytes or null if the input cannot be decoded.

These tests are used for `data:` URLs in this directory (see `base64.any.js`) and `window.atob()` in `../../html/webappapis/atob/base64.html`.
