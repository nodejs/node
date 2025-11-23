# Node.js

Node.js is an open-source, cross-platform JavaScript runtime.

For documentation, guides, and downloads, visit the [Node.js website][].  
The project operates under an open governance model and is supported by the [OpenJS Foundation][].

Contributors are expected to collaborate respectfully and help move the project forward.  
The [TSC](./GOVERNANCE.md#technical-steering-committee) may limit participation from anyone who repeatedly disrupts the community.

**This project follows the [Code of Conduct][].**

## Table of contents
- [Support](#support)
- [Release types](#release-types)
- [Download](#download)
- [Verifying binaries](#verifying-binaries)
- [Building Node.js](#building-nodejs)
- [Security](#security)
- [Contributing to Node.js](#contributing-to-nodejs)
- [Current project team members](#current-project-team-members)
- [License](#license)

## Support
If you need help using Node.js, see the [support guidelines](.github/SUPPORT.md).

## Release types

### Current
Actively developed. A new major version ships every April and October.  
April releases transition to LTS in October.

### LTS
Even-numbered major versions become LTS.  
Each receives 12 months of Active LTS and 18 months of Maintenance.  
No breaking changes unless necessary.

### Nightly
Daily builds from the Current branch. Intended for testing.

All Current and LTS releases follow semantic versioning.  
Each signed release is produced by a member of the Release Team.  
More information is available in the [Release README](https://github.com/nodejs/Release#readme).

## Download

Official downloads:  
https://nodejs.org/en/download/

- Latest Current: https://nodejs.org/download/release/latest/  
- Latest for each LTS line: https://nodejs.org/download/release/latest-<codename>/  
- Nightly builds: https://nodejs.org/download/nightly/

API documentation: https://nodejs.org/api/

## Verifying binaries

Each download directory includes `SHASUMS256.txt.asc` with checksums and a PGP signature.

Download the project keyring:

```bash
curl -fsLo "./nodejs-keyring.kbx" "https://github.com/nodejs/release-keys/raw/HEAD/gpg/pubring.kbx"
```

Verify downloaded files:

```bash
curl -fsO "https://nodejs.org/dist/${VERSION}/SHASUMS256.txt.asc" && gpgv --keyring="./nodejs-keyring.kbx" --output SHASUMS256.txt < SHASUMS256.txt.asc && shasum --check SHASUMS256.txt --ignore-missing
```

## Building Node.js
See [BUILDING.md](BUILDING.md) for build instructions and supported platforms.

## Security
To report a security issue, see [SECURITY.md](./SECURITY.md).

## Contributing to Node.js
- [Contributing to the project][]  
- [Working Groups][]  
- [Strategic initiatives][]  
- [Technical values and prioritization][]

## Current project team members
Team structure and governance details are documented in [GOVERNANCE.md](./GOVERNANCE.md).  
Lists of TSC members, collaborators, triagers, and release keys are maintained in this repository.

## License

Node.js is licensed under the MIT License.  
See the full [LICENSE](https://github.com/nodejs/node/blob/main/LICENSE) for third-party license information.

[Code of Conduct]: https://github.com/nodejs/admin/blob/HEAD/CODE_OF_CONDUCT.md  
[Node.js website]: https://nodejs.org/  
[OpenJS Foundation]: https://openjsf.org/  
[Contributing to the project]: CONTRIBUTING.md  
[Working Groups]: https://github.com/nodejs/TSC/blob/HEAD/WORKING_GROUPS.md  
[Strategic initiatives]: doc/contributing/strategic-initiatives.md  
[Technical values and prioritization]: doc/contributing/technical-values.md
