/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/load-all-commands.js TAP load each command access > must match snapshot 1`] = `
Set access level on published packages

Usage:
npm access public [<package>]
npm access restricted [<package>]
npm access grant <read-only|read-write> <scope:team> [<package>]
npm access revoke <scope:team> [<package>]
npm access 2fa-required [<package>]
npm access 2fa-not-required [<package>]
npm access ls-packages [<user>|<scope>|<scope:team>]
npm access ls-collaborators [<package> [<user>]]
npm access edit [<package>]

Options:
[--registry <registry>] [--otp <otp>]

Run "npm help access" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command adduser > must match snapshot 1`] = `
Add a registry user account

Usage:
npm adduser

Options:
[--registry <registry>] [--scope <@scope>]
[--auth-type <legacy|webauthn|sso|saml|oauth>]

aliases: login, add-user

Run "npm help adduser" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command audit > must match snapshot 1`] = `
Run a security audit

Usage:
npm audit [fix]

Options:
[--audit-level <info|low|moderate|high|critical|none>] [--dry-run] [-f|--force]
[--json] [--package-lock-only]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]]
[--foreground-scripts] [--ignore-scripts]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--install-links]

Run "npm help audit" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command bin > must match snapshot 1`] = `
Display npm bin folder

Usage:
npm bin

Options:
[-g|--global]

Run "npm help bin" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command birthday > must match snapshot 1`] = `
Birthday, deprecated

Usage:
npm birthday

Run "npm help birthday" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command bugs > must match snapshot 1`] = `
Report bugs for a package in a web browser

Usage:
npm bugs [<pkgname> [<pkgname> ...]]

Options:
[--no-browser|--browser <browser>] [--registry <registry>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root]

alias: issues

Run "npm help bugs" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command cache > must match snapshot 1`] = `
Manipulates packages cache

Usage:
npm cache add <tarball file>
npm cache add <folder>
npm cache add <tarball url>
npm cache add <git url>
npm cache add <name>@<version>
npm cache clean [<key>]
npm cache ls [<name>@<version>]
npm cache verify

Options:
[--cache <cache>]

Run "npm help cache" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command ci > must match snapshot 1`] = `
Clean install a project

Usage:
npm ci

Options:
[--no-audit] [--foreground-scripts] [--ignore-scripts]
[--script-shell <script-shell>]

aliases: clean-install, ic, install-clean, isntall-clean

Run "npm help ci" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command completion > must match snapshot 1`] = `
Tab Completion for npm

Usage:
npm completion

Run "npm help completion" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command config > must match snapshot 1`] = `
Manage the npm configuration files

Usage:
npm config set <key>=<value> [<key>=<value> ...]
npm config get [<key> [<key> ...]]
npm config delete <key> [<key> ...]
npm config list [--json]
npm config edit

Options:
[--json] [-g|--global] [--editor <editor>] [-L|--location <global|user|project>]
[-l|--long]

alias: c

Run "npm help config" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command dedupe > must match snapshot 1`] = `
Reduce duplication in the package tree

Usage:
npm dedupe

Options:
[--global-style] [--legacy-bundling] [--strict-peer-deps] [--no-package-lock]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--ignore-scripts]
[--no-audit] [--no-bin-links] [--no-fund] [--dry-run]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--install-links]

alias: ddp

Run "npm help dedupe" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command deprecate > must match snapshot 1`] = `
Deprecate a version of a package

Usage:
npm deprecate <pkg>[@<version>] <message>

Options:
[--registry <registry>] [--otp <otp>]

Run "npm help deprecate" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command diff > must match snapshot 1`] = `
The registry diff command

Usage:
npm diff [...<paths>]

Options:
[--diff <pkg-name|spec|version> [--diff <pkg-name|spec|version> ...]]
[--diff-name-only] [--diff-unified <number>] [--diff-ignore-all-space]
[--diff-no-prefix] [--diff-src-prefix <path>] [--diff-dst-prefix <path>]
[--diff-text] [-g|--global] [--tag <tag>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root]

Run "npm help diff" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command dist-tag > must match snapshot 1`] = `
Modify package distribution tags

Usage:
npm dist-tag add <pkg>@<version> [<tag>]
npm dist-tag rm <pkg> <tag>
npm dist-tag ls [<pkg>]

Options:
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root]

alias: dist-tags

Run "npm help dist-tag" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command docs > must match snapshot 1`] = `
Open documentation for a package in a web browser

Usage:
npm docs [<pkgname> [<pkgname> ...]]

Options:
[--no-browser|--browser <browser>] [--registry <registry>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root]

alias: home

Run "npm help docs" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command doctor > must match snapshot 1`] = `
Check your npm environment

Usage:
npm doctor

Options:
[--registry <registry>]

Run "npm help doctor" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command edit > must match snapshot 1`] = `
Edit an installed package

Usage:
npm edit <pkg>[/<subpkg>...]

Options:
[--editor <editor>]

Run "npm help edit" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command exec > must match snapshot 1`] = `
Run a command from a local or remote npm package

Usage:
npm exec -- <pkg>[@<version>] [args...]
npm exec --package=<pkg>[@<version>] -- <cmd> [args...]
npm exec -c '<cmd> [args...]'
npm exec --package=foo -c '<cmd> [args...]'

Options:
[--package <pkg>[@<version>] [--package <pkg>[@<version>] ...]]
[-c|--call <call>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root]

alias: x

Run "npm help exec" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command explain > must match snapshot 1`] = `
Explain installed packages

Usage:
npm explain <folder | specifier>

Options:
[--json] [-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]

alias: why

Run "npm help explain" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command explore > must match snapshot 1`] = `
Browse an installed package

Usage:
npm explore <pkg> [ -- <command>]

Options:
[--shell <shell>]

Run "npm help explore" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command find-dupes > must match snapshot 1`] = `
Find duplication in the package tree

Usage:
npm find-dupes

Options:
[--global-style] [--legacy-bundling] [--strict-peer-deps] [--no-package-lock]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--ignore-scripts]
[--no-audit] [--no-bin-links] [--no-fund]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--install-links]

Run "npm help find-dupes" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command fund > must match snapshot 1`] = `
Retrieve funding information

Usage:
npm fund [[<@scope>/]<pkg>]

Options:
[--json] [--no-browser|--browser <browser>] [--unicode]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[--which <fundingSourceNumber>]

Run "npm help fund" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command get > must match snapshot 1`] = `
Get a value from the npm configuration

Usage:
npm get [<key> ...] (See \`npm config\`)

Run "npm help get" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command help > must match snapshot 1`] = `
Get help on npm

Usage:
npm help <term> [<terms..>]

Options:
[--viewer <viewer>]

alias: hlep

Run "npm help help" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command help-search > must match snapshot 1`] = `
Search npm help documentation

Usage:
npm help-search <text>

Options:
[-l|--long]

Run "npm help help-search" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command hook > must match snapshot 1`] = `
Manage registry hooks

Usage:
npm hook add <pkg> <url> <secret> [--type=<type>]
npm hook ls [pkg]
npm hook rm <id>
npm hook update <id> <url> <secret>

Options:
[--registry <registry>] [--otp <otp>]

Run "npm help hook" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command init > must match snapshot 1`] = `
Create a package.json file

Usage:
npm init [--force|-f|--yes|-y|--scope]
npm init <@scope> (same as \`npx <@scope>/create\`)
npm init [<@scope>/]<name> (same as \`npx [<@scope>/]create-<name>\`)

Options:
[-y|--yes] [-f|--force]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--no-workspaces-update] [--include-workspace-root]

aliases: create, innit

Run "npm help init" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command install > must match snapshot 1`] = `
Install a package

Usage:
npm install [<@scope>/]<pkg>
npm install [<@scope>/]<pkg>@<tag>
npm install [<@scope>/]<pkg>@<version>
npm install [<@scope>/]<pkg>@<version range>
npm install <alias>@npm:<name>
npm install <folder>
npm install <tarball file>
npm install <tarball url>
npm install <git:// url>
npm install <github username>/<github project>

Options:
[-S|--save|--no-save|--save-prod|--save-dev|--save-optional|--save-peer|--save-bundle]
[-E|--save-exact] [-g|--global] [--global-style] [--legacy-bundling]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]]
[--strict-peer-deps] [--no-package-lock] [--foreground-scripts]
[--ignore-scripts] [--no-audit] [--no-bin-links] [--no-fund] [--dry-run]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--install-links]

aliases: add, i, in, ins, inst, insta, instal, isnt, isnta, isntal, isntall

Run "npm help install" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command install-ci-test > must match snapshot 1`] = `
Install a project with a clean slate and run tests

Usage:
npm install-ci-test

Options:
[--no-audit] [--foreground-scripts] [--ignore-scripts]
[--script-shell <script-shell>]

alias: cit

Run "npm help install-ci-test" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command install-test > must match snapshot 1`] = `
Install package(s) and run tests

Usage:
npm install-test [<@scope>/]<pkg>
npm install-test [<@scope>/]<pkg>@<tag>
npm install-test [<@scope>/]<pkg>@<version>
npm install-test [<@scope>/]<pkg>@<version range>
npm install-test <alias>@npm:<name>
npm install-test <folder>
npm install-test <tarball file>
npm install-test <tarball url>
npm install-test <git:// url>
npm install-test <github username>/<github project>

Options:
[-S|--save|--no-save|--save-prod|--save-dev|--save-optional|--save-peer|--save-bundle]
[-E|--save-exact] [-g|--global] [--global-style] [--legacy-bundling]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]]
[--strict-peer-deps] [--no-package-lock] [--foreground-scripts]
[--ignore-scripts] [--no-audit] [--no-bin-links] [--no-fund] [--dry-run]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--install-links]

alias: it

Run "npm help install-test" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command link > must match snapshot 1`] = `
Symlink a package folder

Usage:
npm link (in package dir)
npm link [<@scope>/]<pkg>[@<version>]

Options:
[-S|--save|--no-save|--save-prod|--save-dev|--save-optional|--save-peer|--save-bundle]
[-E|--save-exact] [-g|--global] [--global-style] [--legacy-bundling]
[--strict-peer-deps] [--no-package-lock]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--ignore-scripts]
[--no-audit] [--no-bin-links] [--no-fund] [--dry-run]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--install-links]

alias: ln

Run "npm help link" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command ll > must match snapshot 1`] = `
List installed packages

Usage:
npm ll [[<@scope>/]<pkg> ...]

Options:
[-a|--all] [--json] [-l|--long] [-p|--parseable] [-g|--global] [--depth <depth>]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--link]
[--package-lock-only] [--unicode]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--install-links]

alias: la

Run "npm help ll" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command login > must match snapshot 1`] = `
Add a registry user account

Usage:
npm adduser

Options:
[--registry <registry>] [--scope <@scope>]
[--auth-type <legacy|webauthn|sso|saml|oauth>]

aliases: login, add-user

Run "npm help adduser" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command logout > must match snapshot 1`] = `
Log out of the registry

Usage:
npm logout

Options:
[--registry <registry>] [--scope <@scope>]

Run "npm help logout" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command ls > must match snapshot 1`] = `
List installed packages

Usage:
npm ls [[<@scope>/]<pkg> ...]

Options:
[-a|--all] [--json] [-l|--long] [-p|--parseable] [-g|--global] [--depth <depth>]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--link]
[--package-lock-only] [--unicode]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--install-links]

alias: list

Run "npm help ls" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command org > must match snapshot 1`] = `
Manage orgs

Usage:
npm org set orgname username [developer | admin | owner]
npm org rm orgname username
npm org ls orgname [<username>]

Options:
[--registry <registry>] [--otp <otp>] [--json] [-p|--parseable]

alias: ogr

Run "npm help org" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command outdated > must match snapshot 1`] = `
Check for outdated packages

Usage:
npm outdated [[<@scope>/]<pkg> ...]

Options:
[-a|--all] [--json] [-l|--long] [-p|--parseable] [-g|--global]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]

Run "npm help outdated" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command owner > must match snapshot 1`] = `
Manage package owners

Usage:
npm owner add <user> [<@scope>/]<pkg>
npm owner rm <user> [<@scope>/]<pkg>
npm owner ls [<@scope>/]<pkg>

Options:
[--registry <registry>] [--otp <otp>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: author

Run "npm help owner" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command pack > must match snapshot 1`] = `
Create a tarball from a package

Usage:
npm pack [[<@scope>/]<pkg>...]

Options:
[--dry-run] [--json] [--pack-destination <pack-destination>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root]

Run "npm help pack" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command ping > must match snapshot 1`] = `
Ping npm registry

Usage:
npm ping

Options:
[--registry <registry>]

Run "npm help ping" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command pkg > must match snapshot 1`] = `
Manages your package.json

Usage:
npm pkg set <key>=<value> [<key>=<value> ...]
npm pkg get [<key> [<key> ...]]
npm pkg delete <key> [<key> ...]
npm pkg set [<array>[<index>].<key>=<value> ...]
npm pkg set [<array>[].<key>=<value> ...]

Options:
[-f|--force] [--json]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

Run "npm help pkg" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command prefix > must match snapshot 1`] = `
Display prefix

Usage:
npm prefix [-g]

Options:
[-g|--global]

Run "npm help prefix" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command profile > must match snapshot 1`] = `
Change settings on your registry profile

Usage:
npm profile enable-2fa [auth-only|auth-and-writes]
npm profile disable-2fa
npm profile get [<key>]
npm profile set <key> <value>

Options:
[--registry <registry>] [--json] [-p|--parseable] [--otp <otp>]

Run "npm help profile" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command prune > must match snapshot 1`] = `
Remove extraneous packages

Usage:
npm prune [[<@scope>/]<pkg>...]

Options:
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--dry-run]
[--json] [--foreground-scripts] [--ignore-scripts]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--install-links]

Run "npm help prune" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command publish > must match snapshot 1`] = `
Publish a package

Usage:
npm publish [<folder>]

Options:
[--tag <tag>] [--access <restricted|public>] [--dry-run] [--otp <otp>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root]

Run "npm help publish" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command rebuild > must match snapshot 1`] = `
Rebuild a package

Usage:
npm rebuild [[<@scope>/]<name>[@<version>] ...]

Options:
[-g|--global] [--no-bin-links] [--foreground-scripts] [--ignore-scripts]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--install-links]

alias: rb

Run "npm help rebuild" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command repo > must match snapshot 1`] = `
Open package repository page in the browser

Usage:
npm repo [<pkgname> [<pkgname> ...]]

Options:
[--no-browser|--browser <browser>] [--registry <registry>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root]

Run "npm help repo" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command restart > must match snapshot 1`] = `
Restart a package

Usage:
npm restart [-- <args>]

Options:
[--ignore-scripts] [--script-shell <script-shell>]

Run "npm help restart" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command root > must match snapshot 1`] = `
Display npm root

Usage:
npm root

Options:
[-g|--global]

Run "npm help root" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command run-script > must match snapshot 1`] = `
Run arbitrary package scripts

Usage:
npm run-script <command> [-- <args>]

Options:
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--if-present] [--ignore-scripts]
[--script-shell <script-shell>]

aliases: run, rum, urn

Run "npm help run-script" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command search > must match snapshot 1`] = `
Search for packages

Usage:
npm search [search terms ...]

Options:
[-l|--long] [--json] [--color|--no-color|--color always] [-p|--parseable]
[--no-description] [--searchopts <searchopts>] [--searchexclude <searchexclude>]
[--registry <registry>] [--prefer-online] [--prefer-offline] [--offline]

aliases: find, s, se

Run "npm help search" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command set > must match snapshot 1`] = `
Set a value in the npm configuration

Usage:
npm set <key>=<value> [<key>=<value> ...] (See \`npm config\`)

Run "npm help set" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command set-script > must match snapshot 1`] = `
Set tasks in the scripts section of package.json, deprecated

Usage:
npm set-script [<script>] [<command>]

Options:
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root]

Run "npm help set-script" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command shrinkwrap > must match snapshot 1`] = `
Lock down dependency versions for publication

Usage:
npm shrinkwrap

Run "npm help shrinkwrap" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command star > must match snapshot 1`] = `
Mark your favorite packages

Usage:
npm star [<pkg>...]

Options:
[--registry <registry>] [--unicode] [--otp <otp>]

Run "npm help star" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command stars > must match snapshot 1`] = `
View packages marked as favorites

Usage:
npm stars [<user>]

Options:
[--registry <registry>]

Run "npm help stars" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command start > must match snapshot 1`] = `
Start a package

Usage:
npm start [-- <args>]

Options:
[--ignore-scripts] [--script-shell <script-shell>]

Run "npm help start" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command stop > must match snapshot 1`] = `
Stop a package

Usage:
npm stop [-- <args>]

Options:
[--ignore-scripts] [--script-shell <script-shell>]

Run "npm help stop" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command team > must match snapshot 1`] = `
Manage organization teams and team memberships

Usage:
npm team create <scope:team> [--otp <otpcode>]
npm team destroy <scope:team> [--otp <otpcode>]
npm team add <scope:team> <user> [--otp <otpcode>]
npm team rm <scope:team> <user> [--otp <otpcode>]
npm team ls <scope>|<scope:team>

Options:
[--registry <registry>] [--otp <otp>] [-p|--parseable] [--json]

Run "npm help team" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command test > must match snapshot 1`] = `
Test a package

Usage:
npm test [-- <args>]

Options:
[--ignore-scripts] [--script-shell <script-shell>]

aliases: tst, t

Run "npm help test" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command token > must match snapshot 1`] = `
Manage your authentication tokens

Usage:
npm token list
npm token revoke <id|token>
npm token create [--read-only] [--cidr=list]

Options:
[--read-only] [--cidr <cidr> [--cidr <cidr> ...]] [--registry <registry>]
[--otp <otp>]

Run "npm help token" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command uninstall > must match snapshot 1`] = `
Remove a package

Usage:
npm uninstall [<@scope>/]<pkg>...

Options:
[-S|--save|--no-save|--save-prod|--save-dev|--save-optional|--save-peer|--save-bundle]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--install-links]

aliases: unlink, remove, rm, r, un

Run "npm help uninstall" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command unpublish > must match snapshot 1`] = `
Remove a package from the registry

Usage:
npm unpublish [<@scope>/]<pkg>[@<version>]

Options:
[--dry-run] [-f|--force]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

Run "npm help unpublish" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command unstar > must match snapshot 1`] = `
Remove an item from your favorite packages

Usage:
npm unstar [<pkg>...]

Options:
[--registry <registry>] [--unicode] [--otp <otp>]

Run "npm help unstar" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command update > must match snapshot 1`] = `
Update packages

Usage:
npm update [<pkg>...]

Options:
[-S|--save|--no-save|--save-prod|--save-dev|--save-optional|--save-peer|--save-bundle]
[-g|--global] [--global-style] [--legacy-bundling]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]]
[--strict-peer-deps] [--no-package-lock] [--foreground-scripts]
[--ignore-scripts] [--no-audit] [--no-bin-links] [--no-fund] [--dry-run]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root] [--install-links]

aliases: up, upgrade, udpate

Run "npm help update" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command version > must match snapshot 1`] = `
Bump a package version

Usage:
npm version [<newversion> | major | minor | patch | premajor | preminor | prepatch | prerelease | from-git]

Options:
[--allow-same-version] [--no-commit-hooks] [--no-git-tag-version] [--json]
[--preid prerelease-id] [--sign-git-tag]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--no-workspaces-update] [--include-workspace-root]

alias: verison

Run "npm help version" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command view > must match snapshot 1`] = `
View registry info

Usage:
npm view [<@scope>/]<pkg>[@<version>] [<field>[.subfield]...]

Options:
[--json] [-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--include-workspace-root]

aliases: info, show, v

Run "npm help view" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command whoami > must match snapshot 1`] = `
Display npm username

Usage:
npm whoami

Options:
[--registry <registry>]

Run "npm help whoami" for more info
`
