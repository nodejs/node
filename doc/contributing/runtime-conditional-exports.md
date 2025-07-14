# Runtime-Specific Conditional Exports in Node.js

Node.js supports conditional exports based on environments such as `"node"`, `"browser"`, and `"import"`. However, runtime-specific conditions like `"electron"` are not included by default and **currently cannot be added dynamically** to the module resolution process.

This guide demonstrates how to simulate runtime-specific behavior without modifying Node.js core.

## Example: Supporting `"electron"` as a Condition

You can use the `--import` flag to preload a script that sets runtime-specific flags.

### Step 1: Preload Script

```js
// register-conditions.js
if (process.versions.electron) {
  process.env.EXPORTS_CONDITION = 'electron';
}
```
### Step 2: Application Code

```js
// index.js
if (process.env.EXPORTS_CONDITION === 'electron') {
  module.exports = require('./electron-specific.js');
} else {
  module.exports = require('./default.js');
}
```
### Step 3: Run with Preload

```bash
node --import ./register-conditions.js index.js
```

### Notes

- This technique does not affect the `exports` field resolution in `package.json`, but allows dynamic control within your application.
- For advanced usage with ESM resolution, consider using a custom loader.

