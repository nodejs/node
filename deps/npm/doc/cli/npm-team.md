npm-team(1) -- Manage organization teams and team memberships
=============================================================

## SYNOPSIS

    npm team create <scope:team>
    npm team destroy <scope:team>

    npm team add <scope:team> <user>
    npm team rm <scope:team> <user>

    npm team ls <scope>|<scope:team>

    npm team edit <scope:team>

## DESCRIPTION

Used to manage teams in organizations, and change team memberships. Does not
handle permissions for packages.

Teams must always be fully qualified with the organization/scope they belong to
when operating on them, separated by a colon (`:`). That is, if you have a
`developers` team on a `foo` organization, you must always refer to that team as
`foo:developers` in these commands.

* create / destroy:
  Create a new team, or destroy an existing one.

* add / rm:
  Add a user to an existing team, or remove a user from a team they belong to.

* ls:
  If performed on an organization name, will return a list of existing teams
  under that organization. If performed on a team, it will instead return a list
  of all users belonging to that particular team.

## DETAILS

`npm team` always operates directly on the current registry, configurable from
the command line using `--registry=<registry url>`.

In order to create teams and manage team membership, you must be a *team admin*
under the given organization. Listing teams and team memberships may be done by
any member of the organizations.

Organization creation and management of team admins and *organization* members
is done through the website, not the npm CLI.

To use teams to manage permissions on packages belonging to your organization,
use the `npm access` command to grant or revoke the appropriate permissions.

## SEE ALSO

* npm-access(1)
* npm-registry(7)
