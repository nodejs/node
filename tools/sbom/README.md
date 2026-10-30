# SBOM generation

Generates a [CycloneDX](https://cyclonedx.org/) Software Bill of Materials
(SBOM) in JSON describing the bundled runtime dependencies of the Node.js
binary.

## Usage

Run `./configure` first. The set of dependencies that end up in the binary is
read from `config.gypi`.

```bash
make sbom       # write out/Release/node-v<version>-sbom.json
make test-sbom  # validate the SBOM written by `make sbom`
```

The script can also be run directly:

```bash
python tools/sbom/generate_sbom.py [--output PATH]
python tools/sbom/generate_sbom.py --validate [--output PATH]
```

## What the SBOM contains

* `metadata.component`: the Node.js binary itself.
* `components`: each bundled dependency that is linked into the binary, with
  name, version and purl. Dependencies excluded by the configure options are
  omitted.
* `dependencies`: a single entry from the Node.js binary to every component.
  Sub-dependencies of bundled dependencies are not recorded, and components
  with no dependencies of their own get no entry.
* `formulation`: build environment details (compilers, glibc version and the
  configure command), when they can be determined. The configure command is
  published as-is and may contain local paths.

A dependency whose version cannot be read is recorded as `unknown` with a
warning rather than failing the build.

## Adding a dependency

Add an entry to `DEP_CATALOG` in `generate_sbom.py` with its name, purl base,
version extractor and, if it is conditional, its scope.
