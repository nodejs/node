npm-help(1) -- Get help on npm
==============================

## SYNOPSIS

    npm help <topic>
    npm help some search terms

## DESCRIPTION

If supplied a topic, then show the appropriate documentation page.

If the topic does not exist, or if multiple terms are provided, then run
the `help-search` command to find a match.  Note that, if `help-search`
finds a single subject, then it will run `help` on that topic, so unique
matches are equivalent to specifying a topic name.

## CONFIGURATION

### viewer

* Default: "man" on Posix, "browser" on Windows
* Type: path

The program to use to view help content.

Set to `"browser"` to view html help content in the default web browser.

## SEE ALSO

* npm(1)
* README
* npm-faq(7)
* npm-folders(7)
* npm-config(1)
* npm-config(7)
* npmrc(5)
* package.json(5)
* npm-help-search(1)
* npm-index(7)
