npm-search(3) -- Search for packages
====================================

## SYNOPSIS

    npm.commands.search(searchTerms, [silent,] [staleness,] callback)

## DESCRIPTION

Search the registry for packages matching the search terms. The available parameters are:

* searchTerms:
  Array of search terms. These terms are case-insensitive.
* silent:
  If true, npm will not log anything to the console.
* staleness:
  This is the threshold for stale packages. "Fresh" packages are not refreshed
  from the registry. This value is measured in seconds.
* callback:
  Returns an object where each key is the name of a package, and the value
  is information about that package along with a 'words' property, which is
  a space-delimited string of all of the interesting words in that package.
  The only properties included are those that are searched, which generally include:

    * name
    * description
    * maintainers
    * url
    * keywords

A search on the registry excludes any result that does not match all of the
search terms. It also removes any items from the results that contain an
excluded term (the "searchexclude" config). The search is case insensitive
and doesn't try to read your mind (it doesn't do any verb tense matching or the
like).
