# Developing Node.js using Dev Containers

Node.js publishes a [nightly image on DockerHub](https://hub.docker.com/r/nodejs/devcontainer) for
[Dev Containers](https://containers.dev/) that can be used to spin up a
development container that comes with pre-installed build dependencies and pre-genreated build cache.

When you need to test a few changes in the main branch and do not need
to change the V8 headers (which is rare), using the nightly image will allow you to compile your
changes from a fresh checkout quickly without having to compile the entire codebase, especially V8,
from scratch.

The Dev Container also allows you to test your changes in a different operating system, and to test
third-party code from bug reports safely with your work-in-progress Node.js branches in an isolated environment.

There are many command line tools, IDEs and services that [support Dev Containers](https://containers.dev/supporting).
Among them, [Visual Studio Code (VS Code)](https://code.visualstudio.com/) is a very popuplar option.
This guide will walk you through the steps to set up a Dev Container for Node.js development using VS Code.
You should be able to use the same [`nodejs/devcontainer:nightly` image](https://hub.docker.com/r/nodejs/devcontainer)
in other tools and services using generally similar steps.

## Prerequisites

Before you begin, ensure you have the following installed on your machine:

* [Docker](https://www.docker.com/get-started)
* [Visual Studio Code](https://code.visualstudio.com/)
* [Dev Containers extension for VS Code](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)

## Setting Up the Dev Container

### 1. Clone the Node.js Repository

If you haven't already, clone the Node.js repository to your local machine.

```bash
git clone https://github.com/nodejs/node.git
```

Or follow [this guide](./pull-requests.md#setting-up-your-local-environment) if you are
setting up the git repository to contribute changes as pull requests.

### 2. Open the Repository in VS Code

Launch VS Code and open the cloned Node.js repository.

### 3. Start the Dev Container

* Press `Ctrl+Shift+P` or `Cmd+Shift+P` to open the command palette.
* Type `Dev Containers: Reopen in Container` and select it.
* Select the appropriate Dev Container configuration from the drop down. The base configuration in this
  repository is `Node.js Dev Container` located in
  [`.devcontainer/base/devcontainer.json`](../../.devcontainer/base/devcontainer.json), which should be
  automatically detected by VS Code.

### 4. Wait for the Container to Build

VS Code will build the Dev Container based on the configuration file. This may take some time depending
on your internet connection and system performance.

After the Dev Container is built, it will start automatically. By default it will run `git restore-mtime` to
restore the modification times of the files in the working directory, in order to keep the build cache valid,
and you will see something like this in the terminal.

```text
Running the postCreateCommand from devcontainer.json...

[10136 ms] Start: Run in container: /bin/sh -c git restore-mtime
```

This may take a few seconds. Wait until it finishes before you open a new terminal.

### 5. Build your changes

Once the Dev Container is running, you can open a terminal in VS Code and run the build commands. By default,
your local repository is mounted inside a checkout in the Dev Container, so any changes you make will be reflected
in the container environment.

In the Dev Container, the necessary dependencies are already installed and Node.js has already been
compiled, so a usable cache will exist. For better developer experience, the
build tool used in the Dev Container is `ninja`, instead of `make`. See
[Building Node.js with Ninja](./building-node-with-ninja.md) for more details on using `ninja` to build Node.js.

```bash
./configure --ninja
ninja -C out/Release
```

As long as your local checkout is not too different from the main branch in the nightly image, the build
should be incremental and fast.

### 6. Leaving the Container

When you're done working in the Dev Container, open the command palette again and select
`Dev Containers: Reopen Folder locally` to go back to your local development environment.

## Customizing the Dev Container

The default configuration is located in
[`.devcontainer/base/devcontainer.json`](../../.devcontainer/base/devcontainer.json) which pairs with the
official [Node.js Nightly Dev Container image](https://github.com/nodejs/devcontainer).
This is tracked in version control. You can create a personal configuration by creating a new
directory in `.devcontainer/` and adding a `devcontainer.json` file there (for example,
`.devcontainer/local/devcontainer.json`), and configure VS Code to use it.

## Rebuilding the Dev Container

Docker will cache the already downloaded Dev Container images. If you wish to pull a new nightly image, you can do
so by running the following command in the terminal on your host machine:

```bash
docker pull nodejs/devcontainer:nightly
```

The default configuration creates a volume to cache the build outputs between Dev Container restarts. If you wish to
clear the build cache, you can do so by deleting the volume named `node-devcontainer-cache`.

```bash
docker volume rm node-devcontainer-cache
```

To rebuild the Dev Container in VS Code, open the command palette and select
`Dev Containers: Rebuild and Reopen in Container`.

## Further Reading

* If you want to propose changes to the official Node.js Nightly Dev Container image, feedback is welcome at
  [nodejs/devcontainer](https://github.com/nodejs/devcontainer). There, you can find the Dockerfile and
  automated nightly workflows.
* [Visual Studio Code Dev Containers Documentation](https://code.visualstudio.com/docs/remote/containers)
* [GitHub's Introduction to Dev Containers](https://docs.github.com/en/codespaces/setting-up-your-project-for-codespaces/adding-a-dev-container-configuration/introduction-to-dev-containers)
* [The Dev Containers Specification](https://containers.dev/implementors/spec/)
