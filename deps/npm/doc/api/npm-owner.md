npm-owner(3) -- Manage package owners
=====================================

## SYNOPSIS

    npm.commands.owner(args, callback)

## DESCRIPTION

The first element of the 'args' parameter defines what to do, and the subsequent
elements depend on the action. Possible values for the action are (order of
parameters are given in parenthesis):

* ls (package):
  List all the users who have access to modify a package and push new versions.
  Handy when you need to know who to bug for help.
* add (user, package):
  Add a new user as a maintainer of a package.  This user is enabled to modify
  metadata, publish new versions, and add other owners.
* rm (user, package):
  Remove a user from the package owner list.  This immediately revokes their
  privileges.

Note that there is only one level of access.  Either you can modify a package,
or you can't.  Future versions may contain more fine-grained access levels, but
that is not implemented at this time.

## SEE ALSO

* npm-publish(3)
* npm-registry(7)
