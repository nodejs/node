# Instructions for maintainers of the node-report project

## Publishing to the npm registry

The node-report project is published as an npm native module

For each publish to npm:

 - update the version property in the package.json file, incrementing the major, minor and patch level as appropriate
 - update the CHANGES.md file with a list of commits since last release
 - commit CHANGES.md and package.json to node-report master branch
 - tag commit with an annotated tag
 - git checkout and npm publish the node-report package

Suggested tooling is the slt-release script documented here: https://github.com/strongloop/strong-tools
