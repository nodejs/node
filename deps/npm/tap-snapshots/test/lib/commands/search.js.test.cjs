/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/search.js TAP empty search results > should have expected search results 1`] = `
No matches found for "foo"
`

exports[`test/lib/commands/search.js TAP search /<name>/--color > should have expected search results with color 1`] = `
NAME                      | DESCRIPTION          | AUTHOR          | DATE       | VERSION  | KEYWORDS
[31mlibnpm[0m                    | Collection of…       | =nlf…           | 2019-07-16 | 3.0.1    | npm api package manager lib
[31mlibnpm[0maccess              | programmatic…        | =nlf…           | 2020-11-03 | 4.0.1    | [31mlibnpm[0maccess
@evocateur/[31mlibnpm[0maccess   | programmatic…        | =evocateur      | 2019-07-16 | 3.1.2    |
@evocateur/[31mlibnpm[0mpublish  | Programmatic API…    | =evocateur      | 2019-07-16 | 1.2.2    |
[31mlibnpm[0morg                 | Programmatic api…    | =nlf…           | 2020-11-03 | 2.0.1    | [31mlibnpm[0m npm package manager api orgs teams
[31mlibnpm[0msearch              | Programmatic API…    | =nlf…           | 2020-12-08 | 3.1.0    | npm search api [31mlibnpm[0m
[31mlibnpm[0mteam                | npm Team management… | =nlf…           | 2020-11-03 | 2.0.2    |
[31mlibnpm[0mhook                | programmatic API…    | =nlf…           | 2020-11-03 | 6.0.1    | npm hooks registry npm api
[31mlibnpm[0mpublish             | Programmatic API…    | =nlf…           | 2020-11-03 | 4.0.0    |
[31mlibnpm[0mfund                | Programmatic API…    | =nlf…           | 2020-12-08 | 1.0.2    | npm npmcli [31mlibnpm[0m cli git fund gitfund
@npmcli/map-workspaces    | Retrieves a…         | =nlf…           | 2020-09-30 | 1.0.1    | npm npmcli [31mlibnpm[0m cli workspaces map-workspaces
[31mlibnpm[0mversion             | library to do the…   | =nlf…           | 2020-11-04 | 1.0.7    |
@types/[31mlibnpm[0msearch       | TypeScript…          | =types          | 2019-09-26 | 2.0.1    |
`

exports[`test/lib/commands/search.js TAP search <name> --color > should have expected search results with color 1`] = `
NAME                      | DESCRIPTION          | AUTHOR          | DATE       | VERSION  | KEYWORDS
[31mlibnpm[0m                    | Collection of…       | =nlf…           | 2019-07-16 | 3.0.1    | npm api package manager lib[31m[0m
[31mlibnpm[0maccess              | programmatic…        | =nlf…           | 2020-11-03 | 4.0.1    | [31mlibnpm[0maccess[31m[0m
@evocateur/[31mlibnpm[0maccess   | programmatic…        | =evocateur      | 2019-07-16 | 3.1.2    | [31m[0m
@evocateur/[31mlibnpm[0mpublish  | Programmatic API…    | =evocateur      | 2019-07-16 | 1.2.2    | [31m[0m
[31mlibnpm[0morg                 | Programmatic api…    | =nlf…           | 2020-11-03 | 2.0.1    | [31mlibnpm[0m npm package manager api orgs teams[31m[0m
[31mlibnpm[0msearch              | Programmatic API…    | =nlf…           | 2020-12-08 | 3.1.0    | npm search api [31mlibnpm[0m[31m[0m
[31mlibnpm[0mteam                | npm Team management… | =nlf…           | 2020-11-03 | 2.0.2    | [31m[0m
[31mlibnpm[0mhook                | programmatic API…    | =nlf…           | 2020-11-03 | 6.0.1    | npm hooks registry npm api[31m[0m
[31mlibnpm[0mpublish             | Programmatic API…    | =nlf…           | 2020-11-03 | 4.0.0    | [31m[0m
[31mlibnpm[0mfund                | Programmatic API…    | =nlf…           | 2020-12-08 | 1.0.2    | npm npmcli [31mlibnpm[0m cli git fund gitfund[31m[0m
@npmcli/map-workspaces    | Retrieves a…         | =nlf…           | 2020-09-30 | 1.0.1    | npm npmcli [31mlibnpm[0m cli workspaces map-workspaces[31m[0m
[31mlibnpm[0mversion             | library to do the…   | =nlf…           | 2020-11-04 | 1.0.7    | [31m[0m
@types/[31mlibnpm[0msearch       | TypeScript…          | =types          | 2019-09-26 | 2.0.1    | [31m[0m
`

exports[`test/lib/commands/search.js TAP search <name> --parseable > should have expected search results as parseable 1`] = `
libnpm	Collection of programmatic APIs for the npm CLI	=nlf =ruyadorno =darcyclarke =isaacs	2019-07-16 	3.0.1	npm api package manager lib
libnpmaccess	programmatic library for \`npm access\` commands	=nlf =ruyadorno =darcyclarke =isaacs	2020-11-03 	4.0.1	libnpmaccess
@evocateur/libnpmaccess	programmatic library for \`npm access\` commands	=evocateur	2019-07-16 	3.1.2
@evocateur/libnpmpublish	Programmatic API for the bits behind npm publish and unpublish	=evocateur	2019-07-16 	1.2.2
libnpmorg	Programmatic api for \`npm org\` commands	=nlf =ruyadorno =darcyclarke =isaacs	2020-11-03 	2.0.1	libnpm npm package manager api orgs teams
libnpmsearch	Programmatic API for searching in npm and compatible registries.	=nlf =ruyadorno =darcyclarke =isaacs	2020-12-08 	3.1.0	npm search api libnpm
libnpmteam	npm Team management APIs	=nlf =ruyadorno =darcyclarke =isaacs	2020-11-03 	2.0.2
libnpmhook	programmatic API for managing npm registry hooks	=nlf =ruyadorno =darcyclarke =isaacs	2020-11-03 	6.0.1	npm hooks registry npm api
libnpmpublish	Programmatic API for the bits behind npm publish and unpublish	=nlf =ruyadorno =darcyclarke =isaacs	2020-11-03 	4.0.0
libnpmfund	Programmatic API for npm fund	=nlf =ruyadorno =darcyclarke =isaacs	2020-12-08 	1.0.2	npm npmcli libnpm cli git fund gitfund
@npmcli/map-workspaces	Retrieves a name:pathname Map for a given workspaces config	=nlf =ruyadorno =darcyclarke =isaacs	2020-09-30 	1.0.1	npm npmcli libnpm cli workspaces map-workspaces
libnpmversion	library to do the things that 'npm version' does	=nlf =ruyadorno =darcyclarke =isaacs	2020-11-04 	1.0.7
@types/libnpmsearch	TypeScript definitions for libnpmsearch	=types	2019-09-26 	2.0.1
`

exports[`test/lib/commands/search.js TAP search <name> > should have filtered expected search results 1`] = `
NAME                      | DESCRIPTION          | AUTHOR          | DATE        | VERSION  | KEYWORDS
foo                       |                      | =foo            | prehistoric | 1.0.0    |
libnpmversion             |                      | =foo            | prehistoric | 1.0.0    |
`

exports[`test/lib/commands/search.js TAP search <name> text > should have expected search results 1`] = `
NAME                      | DESCRIPTION          | AUTHOR          | DATE       | VERSION  | KEYWORDS
libnpm                    | Collection of…       | =nlf…           | 2019-07-16 | 3.0.1    | npm api package manager lib
libnpmaccess              | programmatic…        | =nlf…           | 2020-11-03 | 4.0.1    | libnpmaccess
@evocateur/libnpmaccess   | programmatic…        | =evocateur      | 2019-07-16 | 3.1.2    |
@evocateur/libnpmpublish  | Programmatic API…    | =evocateur      | 2019-07-16 | 1.2.2    |
libnpmorg                 | Programmatic api…    | =nlf…           | 2020-11-03 | 2.0.1    | libnpm npm package manager api orgs teams
libnpmsearch              | Programmatic API…    | =nlf…           | 2020-12-08 | 3.1.0    | npm search api libnpm
libnpmteam                | npm Team management… | =nlf…           | 2020-11-03 | 2.0.2    |
libnpmhook                | programmatic API…    | =nlf…           | 2020-11-03 | 6.0.1    | npm hooks registry npm api
libnpmpublish             | Programmatic API…    | =nlf…           | 2020-11-03 | 4.0.0    |
libnpmfund                | Programmatic API…    | =nlf…           | 2020-12-08 | 1.0.2    | npm npmcli libnpm cli git fund gitfund
@npmcli/map-workspaces    | Retrieves a…         | =nlf…           | 2020-09-30 | 1.0.1    | npm npmcli libnpm cli workspaces map-workspaces
libnpmversion             | library to do the…   | =nlf…           | 2020-11-04 | 1.0.7    |
@types/libnpmsearch       | TypeScript…          | =types          | 2019-09-26 | 2.0.1    |
`

exports[`test/lib/commands/search.js TAP search exclude forward slash > results should not have libnpmversion 1`] = `
NAME                      | DESCRIPTION          | AUTHOR          | DATE       | VERSION  | KEYWORDS
libnpm                    | Collection of…       | =nlf…           | 2019-07-16 | 3.0.1    | npm api package manager lib
libnpmaccess              | programmatic…        | =nlf…           | 2020-11-03 | 4.0.1    | libnpmaccess
@evocateur/libnpmaccess   | programmatic…        | =evocateur      | 2019-07-16 | 3.1.2    |
@evocateur/libnpmpublish  | Programmatic API…    | =evocateur      | 2019-07-16 | 1.2.2    |
libnpmorg                 | Programmatic api…    | =nlf…           | 2020-11-03 | 2.0.1    | libnpm npm package manager api orgs teams
libnpmsearch              | Programmatic API…    | =nlf…           | 2020-12-08 | 3.1.0    | npm search api libnpm
libnpmteam                | npm Team management… | =nlf…           | 2020-11-03 | 2.0.2    |
libnpmhook                | programmatic API…    | =nlf…           | 2020-11-03 | 6.0.1    | npm hooks registry npm api
libnpmpublish             | Programmatic API…    | =nlf…           | 2020-11-03 | 4.0.0    |
libnpmfund                | Programmatic API…    | =nlf…           | 2020-12-08 | 1.0.2    | npm npmcli libnpm cli git fund gitfund
@npmcli/map-workspaces    | Retrieves a…         | =nlf…           | 2020-09-30 | 1.0.1    | npm npmcli libnpm cli workspaces map-workspaces
@types/libnpmsearch       | TypeScript…          | =types          | 2019-09-26 | 2.0.1    |
`

exports[`test/lib/commands/search.js TAP search exclude regex > results should not have libnpmversion 1`] = `
NAME                      | DESCRIPTION          | AUTHOR          | DATE       | VERSION  | KEYWORDS
libnpm                    | Collection of…       | =nlf…           | 2019-07-16 | 3.0.1    | npm api package manager lib
libnpmaccess              | programmatic…        | =nlf…           | 2020-11-03 | 4.0.1    | libnpmaccess
@evocateur/libnpmaccess   | programmatic…        | =evocateur      | 2019-07-16 | 3.1.2    |
@evocateur/libnpmpublish  | Programmatic API…    | =evocateur      | 2019-07-16 | 1.2.2    |
libnpmorg                 | Programmatic api…    | =nlf…           | 2020-11-03 | 2.0.1    | libnpm npm package manager api orgs teams
libnpmsearch              | Programmatic API…    | =nlf…           | 2020-12-08 | 3.1.0    | npm search api libnpm
libnpmteam                | npm Team management… | =nlf…           | 2020-11-03 | 2.0.2    |
libnpmhook                | programmatic API…    | =nlf…           | 2020-11-03 | 6.0.1    | npm hooks registry npm api
libnpmpublish             | Programmatic API…    | =nlf…           | 2020-11-03 | 4.0.0    |
libnpmfund                | Programmatic API…    | =nlf…           | 2020-12-08 | 1.0.2    | npm npmcli libnpm cli git fund gitfund
@npmcli/map-workspaces    | Retrieves a…         | =nlf…           | 2020-09-30 | 1.0.1    | npm npmcli libnpm cli workspaces map-workspaces
@types/libnpmsearch       | TypeScript…          | =types          | 2019-09-26 | 2.0.1    |
`

exports[`test/lib/commands/search.js TAP search exclude string > results should not have libnpmversion 1`] = `
NAME                      | DESCRIPTION          | AUTHOR          | DATE       | VERSION  | KEYWORDS
libnpm                    | Collection of…       | =nlf…           | 2019-07-16 | 3.0.1    | npm api package manager lib
libnpmaccess              | programmatic…        | =nlf…           | 2020-11-03 | 4.0.1    | libnpmaccess
@evocateur/libnpmaccess   | programmatic…        | =evocateur      | 2019-07-16 | 3.1.2    |
@evocateur/libnpmpublish  | Programmatic API…    | =evocateur      | 2019-07-16 | 1.2.2    |
libnpmorg                 | Programmatic api…    | =nlf…           | 2020-11-03 | 2.0.1    | libnpm npm package manager api orgs teams
libnpmsearch              | Programmatic API…    | =nlf…           | 2020-12-08 | 3.1.0    | npm search api libnpm
libnpmteam                | npm Team management… | =nlf…           | 2020-11-03 | 2.0.2    |
libnpmhook                | programmatic API…    | =nlf…           | 2020-11-03 | 6.0.1    | npm hooks registry npm api
libnpmpublish             | Programmatic API…    | =nlf…           | 2020-11-03 | 4.0.0    |
libnpmfund                | Programmatic API…    | =nlf…           | 2020-12-08 | 1.0.2    | npm npmcli libnpm cli git fund gitfund
@npmcli/map-workspaces    | Retrieves a…         | =nlf…           | 2020-09-30 | 1.0.1    | npm npmcli libnpm cli workspaces map-workspaces
@types/libnpmsearch       | TypeScript…          | =types          | 2019-09-26 | 2.0.1    |
`

exports[`test/lib/commands/search.js TAP search exclude username with upper case letters > results should not have nlf 1`] = `
NAME                      | DESCRIPTION          | AUTHOR          | DATE       | VERSION  | KEYWORDS
@evocateur/libnpmaccess   | programmatic…        | =evocateur      | 2019-07-16 | 3.1.2    |
@evocateur/libnpmpublish  | Programmatic API…    | =evocateur      | 2019-07-16 | 1.2.2    |
@types/libnpmsearch       | TypeScript…          | =types          | 2019-09-26 | 2.0.1    |
`
