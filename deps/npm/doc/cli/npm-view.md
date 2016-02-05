npm-view(1) -- View registry info
=================================

## SYNOPSIS

    npm view [@<scope>/]<name>[@<version>] [<field>[.<subfield>]...]
    npm v [@<scope>/]<name>[@<version>] [<field>[.<subfield>]...]

## DESCRIPTION

This command shows data about a package and prints it to the stream
referenced by the `outfd` config, which defaults to stdout.

To show the package registry entry for the `connect` package, you can do
this:

    npm view connect

The default version is "latest" if unspecified.

Field names can be specified after the package descriptor.
For example, to show the dependencies of the `ronn` package at version
0.3.5, you could do the following:

    npm view ronn@0.3.5 dependencies

You can view child fields by separating them with a period.
To view the git repository URL for the latest version of npm, you could
do this:

    npm view npm repository.url

This makes it easy to view information about a dependency with a bit of
shell scripting.  For example, to view all the data about the version of
opts that ronn depends on, you can do this:

    npm view opts@$(npm view ronn dependencies.opts)

For fields that are arrays, requesting a non-numeric field will return
all of the values from the objects in the list.  For example, to get all
the contributor names for the "express" project, you can do this:

    npm view express contributors.email

You may also use numeric indices in square braces to specifically select
an item in an array field.  To just get the email address of the first
contributor in the list, you can do this:

    npm view express contributors[0].email

Multiple fields may be specified, and will be printed one after another.
For exampls, to get all the contributor names and email addresses, you
can do this:

    npm view express contributors.name contributors.email

"Person" fields are shown as a string if they would be shown as an
object.  So, for example, this will show the list of npm contributors in
the shortened string format.  (See `package.json(5)` for more on this.)

    npm view npm contributors

If a version range is provided, then data will be printed for every
matching version of the package.  This will show which version of jsdom
was required by each matching version of yui3:

    npm view yui3@'>0.5.4' dependencies.jsdom

To show the `connect` package version history, you can do
this:

    npm view connect versions

## OUTPUT

If only a single string field for a single version is output, then it
will not be colorized or quoted, so as to enable piping the output to
another command. If the field is an object, it will be output as a JavaScript object literal.

If the --json flag is given, the outputted fields will be JSON.

If the version range matches multiple versions, than each printed value
will be prefixed with the version it applies to.

If multiple fields are requested, than each of them are prefixed with
the field name.

## SEE ALSO

* npm-search(1)
* npm-registry(7)
* npm-config(1)
* npm-config(7)
* npmrc(5)
* npm-docs(1)
