npm-owner(1) -- Manage package owners
=====================================

## SYNOPSIS

    npm owner add <user> [<@scope>/]<pkg>
    npm owner rm <user> [<@scope>/]<pkg>
    npm owner ls [<@scope>/]<pkg>

    aliases: author

## DESCRIPTION

Manage ownership of published packages.

* ls:
  List all the users who have access to modify a package and push new versions.
  Handy when you need to know who to bug for help.
* add:
  Add a new user as a maintainer of a package.  This user is enabled to modify
  metadata, publish new versions, and add other owners.
* rm:
  Remove a user from the package owner list.  This immediately revokes their
  privileges.

Note that there is only one level of access.  Either you can modify a package,
or you can't.  Future versions may contain more fine-grained access levels, but
that is not implemented at this time.

## SEE ALSO

* npm-publish(1)
* npm-registry(7)
* npm-adduser(1)
* npm-disputes(7)
