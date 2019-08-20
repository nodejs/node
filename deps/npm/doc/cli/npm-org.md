npm-org(1) -- Manage orgs
===================================

## SYNOPSIS

    npm org set <orgname> <username> [developer | admin | owner]
    npm org rm <orgname> <username>
    npm org ls <orgname> [<username>]

## EXAMPLE

Add a new developer to an org:
```
$ npm org set my-org @mx-smith
```

Add a new admin to an org (or change a developer to an admin):
```
$ npm org set my-org @mx-santos admin
```

Remove a user from an org:
```
$ npm org rm my-org mx-santos
```

List all users in an org:
```
$ npm org ls my-org
```

List all users in JSON format:
```
$ npm org ls my-org --json
```

See what role a user has in an org:
```
$ npm org ls my-org @mx-santos
```

## DESCRIPTION

You can use the `npm org` commands to manage and view users of an organization.
It supports adding and removing users, changing their roles, listing them, and
finding specific ones and their roles.

## SEE ALSO

* [Documentation on npm Orgs](https://docs.npmjs.com/orgs/)
