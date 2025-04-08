# How to write documentation for the Node.js project

This document refers to the Node.js API documentation that gets deployed to [nodejs.org/en/docs][]
and consists in a general reference on how to write and update such documentation.

## Style Guide

For a style guide on how to write or update the Node.js documentation refer to the [doc/README][] document.

## Building

There are a few different commands that you can use to build and view the documentation locally,
the simplest one being:

```bash
make docserve
```

This command builds the documentation, spins up a local server and provides you with a URL to
it that you can navigate to in order to view the built documentation.

For more build options refer to the [documentation building][building-the-documentation] documentation.

And for more details about the tooling used to build the documentation refer to
the [API Documentation Tooling][] document.

## Linting and Formatting

To make sure that your changes pass linting run the following command:

```bash
make lint-md
```

[API Documentation Tooling]: ./api-documentation.md
[building-the-documentation]: ../../BUILDING.md#building-the-documentation
[doc/README]: ../../doc/README.md
[nodejs.org/en/docs]: https://nodejs.org/en/docs/
