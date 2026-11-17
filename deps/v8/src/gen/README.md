# Generated files

This directory contains generated files committed to the repository. Do not
edit them manually.

| Files | Generator |
| --- | --- |
| `<config>/static-roots.h` | mksnapshot |
| `intl-wasm/builtin-definitions.h` | Torque |
| `intl-wasm/class-forward-declarations.h` | Torque |
| `intl-wasm/interface-descriptors.inc` | Torque |
| `intl-wasm/bytecodes-builtins-list.h` | bytecode builtins list generator |
| `intl-wasm/instance-types.h` | metagen |

Static roots have four configurations: `intl-wasm`, `intl-nowasm`,
`nointl-wasm`, and `nointl-nowasm`.

Regenerate static roots:

```sh
tools/dev/gen-static-roots.py
```

Regenerate the other headers using the default build configuration:

```sh
tools/dev/gm.py gen-checked-in-headers
```
