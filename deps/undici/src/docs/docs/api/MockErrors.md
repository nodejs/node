# MockErrors

<!--introduced_in=v4.0.0-->
<!--type=module-->
<!-- source_link=lib/mock/mock-errors.js -->

> Stability: 2 - Stable

undici raises a dedicated error when a mocked request cannot be matched against
any registered interceptor. The mock error classes are exposed through the
`mockErrors` namespace of the package:

```js
import { mockErrors } from 'undici'

if (err instanceof mockErrors.MockNotMatchedError) {
  // handle an unmatched mocked request
}
```

Like every other undici error, the classes in this namespace extend
[`UndiciError`][] and carry a stable `code` string, so failures can be
distinguished programmatically without relying on the error message.

## Class: `MockNotMatchedError`

<!-- YAML
added: v4.0.0
-->

* Extends: {UndiciError}

Thrown when a request issued against a [`MockAgent`][] does not match any of the
registered mock dispatches and the agent is configured to disable net connect.

* `name` {string} Always `'MockNotMatchedError'`.
* `code` {string} Always `'UND_MOCK_ERR_MOCK_NOT_MATCHED'`.

### `new MockNotMatchedError([message])`

<!-- YAML
added: v4.0.0
-->

* `message` {string} The error message. **Default:** `'The request does not match any registered mock dispatches'`.

Creates a new `MockNotMatchedError`. When `message` is omitted, a default
message describing the unmatched request is used.

```js
import { mockErrors } from 'undici'

try {
  // ... perform a request that no mock matches ...
} catch (err) {
  if (err.code === 'UND_MOCK_ERR_MOCK_NOT_MATCHED') {
    console.error('No registered mock matched the request')
  }
}
```

[`MockAgent`]: MockAgent.md#class-mockagent
[`UndiciError`]: Errors.md#class-undicierror
