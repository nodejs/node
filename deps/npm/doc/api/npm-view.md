npm-view(3) -- View registry info
=================================

## SYNOPSIS

    npm.commands.view(args, [silent,] callback)

## DESCRIPTION

This command shows data about a package and prints it to the stream
referenced by the `outfd` config, which defaults to stdout.

The "args" parameter is an ordered list that closely resembles the command-line
usage. The elements should be ordered such that the first element is
the package and version (package@version). The version is optional. After that,
the rest of the parameters are fields with optional subfields ("field.subfield")
which can be used to get only the information desired from the registry.

The callback will be passed all of the data returned by the query.

For example, to get the package registry entry for the `connect` package,
you can do this:

    npm.commands.view(["connect"], callback)

If no version is specified, "latest" is assumed.

Field names can be specified after the package descriptor.
For example, to show the dependencies of the `ronn` package at version
0.3.5, you could do the following:

    npm.commands.view(["ronn@0.3.5", "dependencies"], callback)

You can view child field by separating them with a period.
To view the git repository URL for the latest version of npm, you could
do this:

    npm.commands.view(["npm", "repository.url"], callback)

For fields that are arrays, requesting a non-numeric field will return
all of the values from the objects in the list.  For example, to get all
the contributor names for the "express" project, you can do this:

    npm.commands.view(["express", "contributors.email"], callback)

You may also use numeric indices in square braces to specifically select
an item in an array field.  To just get the email address of the first
contributor in the list, you can do this:

    npm.commands.view(["express", "contributors[0].email"], callback)

Multiple fields may be specified, and will be printed one after another.
For exampls, to get all the contributor names and email addresses, you
can do this:

    npm.commands.view(["express", "contributors.name", "contributors.email"], callback)

"Person" fields are shown as a string if they would be shown as an
object.  So, for example, this will show the list of npm contributors in
the shortened string format.  (See `npm help json` for more on this.)

    npm.commands.view(["npm", "contributors"], callback)

If a version range is provided, then data will be printed for every
matching version of the package.  This will show which version of jsdom
was required by each matching version of yui3:

    npm.commands.view(["yui3@'>0.5.4'", "dependencies.jsdom"], callback)

## OUTPUT

If only a single string field for a single version is output, then it
will not be colorized or quoted, so as to enable piping the output to
another command.

If the version range matches multiple versions, than each printed value
will be prefixed with the version it applies to.

If multiple fields are requested, than each of them are prefixed with
the field name.

Console output can be disabled by setting the 'silent' parameter to true.

## RETURN VALUE

The data returned will be an object in this formation:

    { <version>:
      { <field>: <value>
      , ... }
    , ... }

corresponding to the list of fields selected.
