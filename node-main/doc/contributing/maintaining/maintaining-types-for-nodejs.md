# Maintaining types for Node.js

While JavaScript is a weakly-typed language, there are some complementary tools
like [TypeScript][] and [Flow][], which allow developers to annotate the source
code of their JavaScript projects. While many people don't annotate their code,
or make use of annotations at all, there are enough who do that the project has
agreed it's important to work towards having [suitable types for end-users][].

## High level approach - maintaining types

There are a number of ways that types could be maintained for Node.js ranging
from shipping them with the Node.js runtime to having them be externally
maintained.

The different options were discussed as part of the [next-10][] effort and it
was agreed that maintaining them externally is the best approach.
Some of the advantages to this approach include:

* Node.js maintainers do not need to be familiar with any given type
  system/technology.
* Types can be updated without requiring Node.js releases.

The agreement was that the ideal flow would be as follows:

* APIs are added/documented in the existing Node.js markdown files.
* Automation in the Node.js project creates a machine readable JSON
  representation of the API from the documentation.
* Automation within external type projects consumes the JSON and automatically
  generates a PR to add the API.

## High level approach - development workflow

The number of people using TypeScript with Node.js is significant enough
that providing a good developer experience is important. While TypeScript
is identified specifically, a secondary goal is that what we provide to improve
development experience with TypeScript would apply to other type
systems and transpiled languages as well.

We have agreed that the approach will **NOT** include bundling TypeScript
tools with Node.js but instead improve the developer experience for how
those tools are installed/configured to work with Node.js.

The high level developer experience we are working towards was captured in the
[next-10 TypeScript mini-summit](https://github.com/nodejs/next-10/pull/150)
and is as follows:

1. When Node.js is started with an entry point that is not a file type that
   Node.js recognizes, for example `node script.ts`, an informative error
   message is printed that directs users to a webpage where they can
   learn how to configure Node.js to support that file type.
   * If the file was a TypeScript file, a TypeScript specific message with a
     reference to a link on Nodejs.org specific on learning how to
     configure TypeScript will be provided.
   * For other file types a generic message and shared webpage will be
     used.
2. Node.js gains support for loading configuration from a file. Most, if not
   all, of the configuration supported by `NODE_OPTIONS` would be
   supported in this file (which might be the `package.json` that lives
   near the entry point file). The webpage with instructions would tell
   users what configuration to put in this file to get Node.js to support
   their file type.
3. When Node.js is run with the correct configuration, either in a file or
   `NODE_OPTIONS` or flags, the unknown file type is executed as expected.

Some additional specifics around the current approach include:

* Loaders already provide a number of the components needed to
  satisfy the requirements above. They already provide the Node.js
  options that are needed to achieve many of the requirements above.
* `package.json` as the location for the config is potentially a good
  choice as Node.js already looks for it as part of startup.
* The implementation chosen should allow for different configuration
  in/for different environments/conditions such as production
  versus development, or different types of hosted environments
  such as serverless vs traditional, etc.; Node.js would not make
  any recommendations or have any expectations as to what the
  separate configuration blocks should be named or what their
  purposes should be, just that a configuration file should have
  the ability to provide different configurations for user-defined
  conditions.
* There is no plan to define a default tsconfig.json for all Node.js users
* We don't have consensus on providing an opinionated default but
  that should be explored after the initial steps are complete.
* It will be important that as part of the messaging around this
  functionality that we avoid confusion that could lead people to ship
  TypeScript files (e.g. `script.ts`) instead of the processed files
  (e.g. `script.js`).

## Generation/Consumption of machine readable JSON files

When you run `make doc` the canonical markdown files used to
document the Node.js APIs in the
[doc/api][]
directory are converted to both an `.html` file and a `.json` file.

As part of the regular build/release process both the `html` and
`json` files are published to [nodejs.org][].

The generator that does the conversion is in the
[tools/doc][]
directory.

## Markdown structure

The constraints required on the markdown files in the
[doc/api][] directory
in order to be able to generate the JSON files are defined in the
[documentation-style-guide][].

## Planned changes (as of Jan 1 2022)

While JSON files are already being generated and published, they are not
structured well enough for them to be easily consumed by the type projects.
Generally external teams need some custom scripts along with manual fixup
afterwards.

There is an ongoing effort to add additional markdown constraints and
then update the flow in order to be able to generate a better
JSON output.

[Flow]: https://flow.org/
[TypeScript]: https://www.typescriptlang.org/
[doc/api]: https://github.com/nodejs/node/tree/HEAD/doc/api
[documentation-style-guide]: https://github.com/nodejs/node/blob/HEAD/doc/README.md#documentation-style-guide
[next-10]: https://github.com/nodejs/next-10/blob/HEAD/meetings/summit-nov-2021.md#suitable-types-for-end-users
[nodejs.org]: https://nodejs.org/en/docs/
[suitable types for end-users]: https://github.com/nodejs/node/blob/HEAD/doc/contributing/technical-priorities.md#suitable-types-for-end-users
[tools/doc]: https://github.com/nodejs/node/tree/HEAD/tools/doc
