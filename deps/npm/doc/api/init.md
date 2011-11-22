npm init(3) -- Interactively create a package.json file
=======================================================

## SYNOPSIS

    npm.commands.init(args, callback)

## DESCRIPTION

This will ask you a bunch of questions, and then write a package.json for you.

It attempts to make reasonable guesses about what you want things to be set to,
and then writes a package.json file with the options you've selected.

If you already have a package.json file, it'll read that first, and default to
the options in there.

It is strictly additive, so it does not delete options from your package.json
without a really good reason to do so.

Since this function expects to be run on the command-line, it doesn't work very
well as a programmatically. The best option is to roll your own, and since
JavaScript makes it stupid simple to output formatted JSON, that is the
preferred method. If you're sure you want to handle command-line prompting,
then go ahead and use this programmatically.

## SEE ALSO

npm-json(1)
