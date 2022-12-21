---
title: npm-team
section: 1
description: Manage organization teams and team memberships
---

### Synopsis

```bash
npm team create <scope:team>
npm team destroy <scope:team>

npm team add <scope:team> <user>
npm team rm <scope:team> <user>

npm team ls <scope>|<scope:team>

npm team edit <scope:team>
```

### Description

Used to manage teams in organizations, and change team memberships. Does not
handle permissions for packages.

Teams must always be fully qualified with the organization/scope they belong to
when operating on them, separated by a colon (`:`). That is, if you have a `wombats` team in a `wisdom` organization, you must always refer to that team as `wisdom:wombats` in these commands.

If you have two-factor authentication enabled in `auth-and-writes` mode, then you can provide a code from your authenticator with `[--otp <otpcode>]`. If you don't include this then you will be prompted.

* create / destroy:
  Create a new team, or destroy an existing one. Note: You cannot remove the `developers` team, <a href="https://docs.npmjs.com/about-developers-team" target="_blank">learn more.</a>
* add / rm:
  Add a user to an existing team, or remove a user from a team they belong to.

* ls:
  If performed on an organization name, will return a list of existing teams
  under that organization. If performed on a team, it will instead return a list
  of all users belonging to that particular team.

* edit:
  Edit a current team.

### Details

`npm team` always operates directly on the current registry, configurable from
the command line using `--registry=<registry url>`.

In order to create teams and manage team membership, you must be a *team admin*
under the given organization. Listing teams and team memberships may be done by
any member of the organizations.

Organization creation and management of team admins and *organization* members
is done through the website, not the npm CLI.

To use teams to manage permissions on packages belonging to your organization,
use the `npm access` command to grant or revoke the appropriate permissions.

### See Also

* [npm access](/commands/npm-access)
* [npm registry](/using-npm/registry)
