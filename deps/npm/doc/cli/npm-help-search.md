npm-help-search(1) -- Search npm help documentation
===================================================

## SYNOPSIS

    npm help-search some search terms

## DESCRIPTION

This command will search the npm markdown documentation files for the
terms provided, and then list the results, sorted by relevance.

If only one result is found, then it will show that help topic.

If the argument to `npm help` is not a known help topic, then it will
call `help-search`.  It is rarely if ever necessary to call this
command directly.

## CONFIGURATION

### long

* Type: Boolean
* Default: false

If true, the "long" flag will cause help-search to output context around
where the terms were found in the documentation.

If false, then help-search will just list out the help topics found.

## SEE ALSO

* npm(1)
* npm-faq(7)
* npm-help(1)
