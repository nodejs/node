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
systems as well.

We have agreed that the approach will `NOT` include bundling TypeScript
tools with Node.js but instead improve the developer experience for how
those tools are installed/configured to work with Node.js.

The high level developer experience we are working towards was captured in the
[next-10 TypeScript mini-summit](https://github.com/nodejs/next-10/pull/150)
and is as follows:

1. Users can ask Node.js to execute a file which is not one of the types it
   can execute by default (.js, .mjs, etc.). For example `node script.ts`.
2. On startup, Node.js will look for a config which is in the scope of the
   file being executed.
3. If no config is found, Node.js will echo either:
   * If the file was a TypeScript file, a TypeScript specific message with a
     reference to a link on Nodejs.org on how to install required components
     for TypeScript and how to add the associated config.
   * If the file was not a TypeScript file, a generic message.
4. If a config is found, Node.js will extract the Node.js options for
   that config and apply them as if they had been provided on the command
   line.
5. Assuming the options associated with the config add support for the
   type of file being loaded, Node.js will execute the file as if it was one
   of the file types it can execute by default.

Some additional specifics around the current approach include:

* Loaders already provide a number of the components needed to
  satisfy the requirements above. They already provide the Node.js
  options that are needed to achieve many of the requirements above.
* package.json as the location for the config is potentially a good
  choice as Node.js already looks for it as part of startup.
* The implementation chosen should allow for difference configuration in
  for different enronments/conditions like prod,dev, etc.
* We don't have consensus on provding an opinionated default but
  that should be explored after the initial steps are complete.
* It will be important that as part of the messaging around this
  functionality that we avoid confusion that could lead people to ship
  TypeScript files (ex `script.ts`) instead of the processed files
  (ex `script.js`).

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
