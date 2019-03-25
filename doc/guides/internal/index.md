# Internal documentation of Node.js

## CLI

### Node.js options:

#### `--inspect-brk-node[=[host:]port]`

<!-- YAML
added: v7.6.0
-->

Activate inspector on `host:port` and break at start of the first internal
JavaScript script executed when the inspector is available.
Default `host:port` is `127.0.0.1:9229`.
