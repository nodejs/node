# npm install-test(1) -- Install package(s) and run tests

## SYNOPSIS

    npm install-test (with no args, in package dir)
    npm install-test [<@scope>/]<name>
    npm install-test [<@scope>/]<name>@<tag>
    npm install-test [<@scope>/]<name>@<version>
    npm install-test [<@scope>/]<name>@<version range>
    npm install-test <tarball file>
    npm install-test <tarball url>
    npm install-test <folder>

    alias: npm it
    common options: [--save|--save-dev|--save-optional] [--save-exact] [--dry-run]

## DESCRIPTION

This command runs an `npm install` followed immediately by an `npm test`. It
takes exactly the same arguments as `npm install`.

## SEE ALSO

- npm-install(1)
- npm-test(1)
