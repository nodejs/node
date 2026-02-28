---
title: npm-trust
section: 1
description: Manage trusted publishing relationships between packages and CI/CD providers
---

### Synopsis

```bash
npm trust
```

Note: This command is unaware of workspaces.

### Prerequisites

Before using npm trust commands, ensure the following requirements are met:

* **npm version**: `npm@11.10.0` or above is required. Use `npm install -g npm@^11.10.0` to update if needed.
* **Write permissions on the package**: You must have write access to the package you're configuring.
* **2FA enabled on account**: Two-factor authentication must be enabled at the account level. Even if it's not currently enabled, you must enable it to use trust commands.
* **Supported authentication methods**: Granular Access Tokens (GAT) with the bypass 2FA option are not supported. Legacy basic auth (username and password) credentials will not work for trust commands or endpoints.
* **Package must exist**: The package you're configuring must already exist on the npm registry.

### Description

Configure trust relationships between npm packages and CI/CD providers using OpenID Connect (OIDC). This is the command-line equivalent of managing trusted publisher configurations on the npm website.

For a comprehensive overview of trusted publishing, see the [npm trusted publishers documentation](https://docs.npmjs.com/trusted-publishers).

The `[package]` argument specifies the package name. If omitted, npm will use the name from the `package.json` in the current directory.

Each trust relationship has its own set of configuration options and flags based on the OIDC claims provided by that provider. OIDC claims come from the CI/CD provider and include information such as repository name, workflow file, or environment. Since each provider's claims differ, the available flags and configuration keys are not universal—npm matches the claims supported by each provider's OIDC configuration. For specific details on which claims and flags are supported for a given provider, use `npm trust <provider> --help`.

The required options depend on the CI/CD provider you're configuring. Detailed information about each option is available in the [managing trusted publisher configurations](https://docs.npmjs.com/trusted-publishers#managing-trusted-publisher-configurations) section of the npm documentation. If a provider is repository-based and the option is not provided, npm will use the `repository.url` field from your `package.json`, if available.

Currently, the registry only supports one configuration per package. If you attempt to create a new trust relationship when one already exists, it will result in an error. To replace an existing configuration:

1. Use `npm trust list [package]` to view the ID of the existing trusted publisher
2. Use `npm trust revoke --id <id> [package]` to remove the existing configuration
3. Then create your new trust relationship

### Bulk Usage

For maintainers managing a large number of packages, you can configure trusted publishing in bulk using bash scripting. Create a loop that iterates through package names and their corresponding configuration details, executing the `npm trust <provider>` command with the `--yes` flag for each package.

The first request will require two-factor authentication. During two-factor authentication, you'll see an option on the npm website to skip two-factor authentication for the next 5 minutes. Enabling this option will allow subsequent `npm trust <provider>` commands to proceed without two-factor authentication, streamlining the bulk configuration process.

We recommend adding a 2-second sleep between each call to avoid rate limiting. With this approach, you can configure approximately 80 packages within the 5-minute two-factor authentication skip window.

### Configuration

### `npm trust github`

Create a trusted relationship between a package and GitHub Actions

#### Synopsis

```bash
npm trust github [package] --file [--repo|--repository] [--env|--environment] [-y|--yes]
```

#### Flags

| Flag | Default | Type | Description |
| --- | --- | --- | --- |
| `--file` | null | String (required) | Name of workflow file within a repositories .GitHub folder (must end in yaml, yml) |
| `--repository`, `--repo` | null | String | Name of the repository in the format owner/repo |
| `--environment`, `--env` | null | String | CI environment name |
| `--dry-run` | false | Boolean | Indicates that you don't want npm to make any changes and that it should       only report what it would have done.  This can be passed into any of the       commands that modify your local installation, eg, `install`,       `update`, `dedupe`, `uninstall`, as well as `pack` and       `publish`.        Note: This is NOT honored by other network related commands, eg       `dist-tags`, `owner`, etc. |
| `--json` | false | Boolean | Whether or not to output JSON data, rather than the normal output.        * In `npm pkg set` it enables parsing set values with JSON.parse()       before saving them to your `package.json`.        Not supported by all npm commands. |
| `--registry` | "https://registry.npmjs.org/" | URL | The base URL of the npm registry. |
| `--yes`, `-y` | null | null or Boolean | Automatically answer "yes" to any prompts that npm might print on       the command line. |

### `npm trust gitlab`

Create a trusted relationship between a package and GitLab CI/CD

#### Synopsis

```bash
npm trust gitlab [package] --file [--project|--repo|--repository] [--env|--environment] [-y|--yes]
```

#### Flags

| Flag | Default | Type | Description |
| --- | --- | --- | --- |
| `--file` | null | String (required) | Name of pipeline file (e.g., .gitlab-ci.yml) |
| `--project` | null | String | Name of the project in the format group/project or group/subgroup/project |
| `--environment`, `--env` | null | String | CI environment name |
| `--dry-run` | false | Boolean | Indicates that you don't want npm to make any changes and that it should       only report what it would have done.  This can be passed into any of the       commands that modify your local installation, eg, `install`,       `update`, `dedupe`, `uninstall`, as well as `pack` and       `publish`.        Note: This is NOT honored by other network related commands, eg       `dist-tags`, `owner`, etc. |
| `--json` | false | Boolean | Whether or not to output JSON data, rather than the normal output.        * In `npm pkg set` it enables parsing set values with JSON.parse()       before saving them to your `package.json`.        Not supported by all npm commands. |
| `--registry` | "https://registry.npmjs.org/" | URL | The base URL of the npm registry. |
| `--yes`, `-y` | null | null or Boolean | Automatically answer "yes" to any prompts that npm might print on       the command line. |

### `npm trust circleci`

Create a trusted relationship between a package and CircleCI

#### Synopsis

```bash
npm trust circleci [package] --org-id <uuid> --project-id <uuid> --pipeline-definition-id <uuid> --vcs-origin <origin> [--context-id <uuid>...] [-y|--yes]
```

#### Flags

| Flag | Default | Type | Description |
| --- | --- | --- | --- |
| `--org-id` | null | String (required) | CircleCI organization UUID |
| `--project-id` | null | String (required) | CircleCI project UUID |
| `--pipeline-definition-id` | null | String (required) | CircleCI pipeline definition UUID |
| `--vcs-origin` | null | String (required) | CircleCI repository origin in format 'provider/owner/repo' |
| `--context-id` | null | null or String (can be set multiple times) | CircleCI context UUID to match |
| `--dry-run` | false | Boolean | Indicates that you don't want npm to make any changes and that it should       only report what it would have done.  This can be passed into any of the       commands that modify your local installation, eg, `install`,       `update`, `dedupe`, `uninstall`, as well as `pack` and       `publish`.        Note: This is NOT honored by other network related commands, eg       `dist-tags`, `owner`, etc. |
| `--json` | false | Boolean | Whether or not to output JSON data, rather than the normal output.        * In `npm pkg set` it enables parsing set values with JSON.parse()       before saving them to your `package.json`.        Not supported by all npm commands. |
| `--registry` | "https://registry.npmjs.org/" | URL | The base URL of the npm registry. |
| `--yes`, `-y` | null | null or Boolean | Automatically answer "yes" to any prompts that npm might print on       the command line. |

### `npm trust list`

List trusted relationships for a package

#### Synopsis

```bash
npm trust list [package]
```

#### Flags

| Flag | Default | Type | Description |
| --- | --- | --- | --- |
| `--json` | false | Boolean | Whether or not to output JSON data, rather than the normal output.        * In `npm pkg set` it enables parsing set values with JSON.parse()       before saving them to your `package.json`.        Not supported by all npm commands. |
| `--registry` | "https://registry.npmjs.org/" | URL | The base URL of the npm registry. |

### `npm trust revoke`

Revoke a trusted relationship for a package

#### Synopsis

```bash
npm trust revoke [package] --id=<trust-id>
```

#### Flags

| Flag | Default | Type | Description |
| --- | --- | --- | --- |
| `--id` | null | String (required) | ID of the trusted relationship to revoke |
| `--dry-run` | false | Boolean | Indicates that you don't want npm to make any changes and that it should       only report what it would have done.  This can be passed into any of the       commands that modify your local installation, eg, `install`,       `update`, `dedupe`, `uninstall`, as well as `pack` and       `publish`.        Note: This is NOT honored by other network related commands, eg       `dist-tags`, `owner`, etc. |
| `--registry` | "https://registry.npmjs.org/" | URL | The base URL of the npm registry. |


### See Also

* [npm publish](/commands/npm-publish)
* [npm token](/commands/npm-token)
* [npm access](/commands/npm-access)
* [npm config](/commands/npm-config)
* [npm registry](/using-npm/registry)
