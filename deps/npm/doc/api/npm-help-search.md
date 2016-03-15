npm-help-search(3) -- Search the help pages
===========================================

## SYNOPSIS

    npm.commands.helpSearch(args, [silent,] callback)

## DESCRIPTION

This command is rarely useful, but it exists in the rare case that it is.

This command takes an array of search terms and returns the help pages that
match in order of best match.

If there is only one match, then npm displays that help section. If there
are multiple results, the results are printed to the screen formatted and the
array of results is returned. Each result is an object with these properties:

* hits:
  A map of args to number of hits on that arg. For example, {"npm": 3}
* found:
  Total number of unique args that matched.
* totalHits:
  Total number of hits.
* lines:
  An array of all matching lines (and some adjacent lines).
* file:
  Name of the file that matched

The silent parameter is not necessary not used, but it may in the future.
