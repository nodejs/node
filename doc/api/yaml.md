# YAML

<!--introduced_in=REPLACEME-->

<!-- YAML
added: REPLACEME
-->

> Stability: 1.0 - Early development

<!-- source_link=lib/yaml.js -->

The `node:yaml` module parses [YAML 1.2][] into JavaScript values. Enable it
with [`--experimental-yaml`][] and import it with `node:`:

```mjs
import yaml from 'node:yaml';
```

```cjs
const yaml = require('node:yaml');
```

Loading the module emits an `ExperimentalWarning`.

## `yaml.parse(input)`

<!-- YAML
added: REPLACEME
-->

* `input` {string|ArrayBuffer|SharedArrayBuffer|TypedArray|DataView} The YAML
  text. Binary input is decoded as UTF-8.
* Returns: {any} The value of the document, or an array with the value of each
  document when the stream holds more than one. An empty stream yields `null`.

`parse()` uses the YAML 1.2 [core schema][]. Plain scalars resolve to null,
booleans, numbers or strings. For example, `yes` and `no` are strings. Quoted
and block scalars are strings unless a tag changes their type, as in
`!!int "42"`. Mappings become plain objects and sequences become arrays, as
with [`JSON.parse()`][].

```mjs
import yaml from 'node:yaml';

const config = yaml.parse(`
name: app
ports:
  - 8080
  - 8443
tls: true
`);
console.log(config);
// { name: 'app', ports: [ 8080, 8443 ], tls: true }

console.log(yaml.parse('--- 1\n--- 2\n'));
// [ 1, 2 ]
```

Aliases to a collection refer to the same object. A collection may contain an
alias to itself, so the result can contain cycles.

The parser applies the core tags `!!str`, `!!int`, `!!float`, `!!bool`,
`!!null`, `!!map` and `!!seq`. A tag must match its value: `!!int abc` throws
an error. Other tags leave collections unchanged and make scalars strings.
For example, `!custom 123` becomes `'123'`.

Mapping keys become property names. Scalar keys use their string form, so
`null`, `true` and `1` become `'null'`, `'true'` and `'1'`. Collection keys use
their JSON text and are limited to 1024 characters. Duplicate keys throw.

Invalid YAML throws an [`ERR_INVALID_YAML`][] `SyntaxError` with the line and
column. The parser rejects disallowed control characters and nesting deeper
than 1000 levels.

[YAML 1.2]: https://yaml.org/spec/1.2.2/
[`--experimental-yaml`]: cli.md#--experimental-yaml
[`ERR_INVALID_YAML`]: errors.md#err_invalid_yaml
[`JSON.parse()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/parse
[core schema]: https://yaml.org/spec/1.2.2/#103-core-schema
