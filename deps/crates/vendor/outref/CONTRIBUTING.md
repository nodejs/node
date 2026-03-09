# Development Guide

## Requirements

|               Toolchain               | Version |
| :-----------------------------------: | :-----: |
|      [Rust](https://rustup.rs/)       | ^1.63.0 |
| [just](https://github.com/casey/just) | ^1.36.0 |

## Workflow

### Download source code

```bash
git clone https://github.com/Nugine/outref.git
cd outref
```

### Run basic checks and tests

```bash
just dev
```

### Open documentation

```bash
just doc
```

### Run CI checks locally

```bash
just ci
```

## Git

### Commit Message

We follow the [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) specification.

### Pull Request

Before creating or updating a pull request, please make sure to run the following commands:

```bash
just dev
```

It's recommended to resolve any warnings or errors before submitting.

Note that some lints may be too restrict. You can allow some of them if you think they are not helpful to the code quality.
