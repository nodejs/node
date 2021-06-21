/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/load-all-commands.js TAP load each command access > must match snapshot 1`] = `
npm access

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
npm adduser

Add a registry user account

Usage:
npm adduser

Options:
[--registry <registry>] [--scope <@scope>]

aliases: login, add-user

Run "npm help adduser" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command audit > must match snapshot 1`] = `
npm audit

Run a security audit

Usage:
npm audit [fix]

Options:
[--audit-level <info|low|moderate|high|critical|none>] [--dry-run] [-f|--force]
[--json] [--package-lock-only]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

Run "npm help audit" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command bin > must match snapshot 1`] = `
npm bin

Display npm bin folder

Usage:
npm bin

Options:
[-g|--global]

Run "npm help bin" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command bugs > must match snapshot 1`] = `
npm bugs

Report bugs for a package in a web browser

Usage:
npm bugs [<pkgname>]

Options:
[--browser|--browser <browser>] [--registry <registry>]

alias: issues

Run "npm help bugs" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command cache > must match snapshot 1`] = `
npm cache

Manipulates packages cache

Usage:
npm cache add <tarball file>
npm cache add <folder>
npm cache add <tarball url>
npm cache add <git url>
npm cache add <name>@<version>
npm cache clean
npm cache verify

Options:
[--cache <cache>]

Run "npm help cache" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command ci > must match snapshot 1`] = `
npm ci

Install a project with a clean slate

Usage:
npm ci

Options:
[--ignore-scripts] [--script-shell <script-shell>]

aliases: clean-install, ic, install-clean, isntall-clean

Run "npm help ci" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command completion > must match snapshot 1`] = `
npm completion

Tab Completion for npm

Usage:
npm completion

Run "npm help completion" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command config > must match snapshot 1`] = `
npm config

Manage the npm configuration files

Usage:
npm config set <key>=<value> [<key>=<value> ...]
npm config get [<key> [<key> ...]]
npm config delete <key> [<key> ...]
npm config list [--json]
npm config edit

Options:
[--json] [-g|--global] [--editor <editor>] [-l|--long]

alias: c

Run "npm help config" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command dedupe > must match snapshot 1`] = `
npm dedupe

Reduce duplication in the package tree

Usage:
npm dedupe

Options:
[--global-style] [--legacy-bundling] [--strict-peer-deps] [--package-lock]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--ignore-scripts]
[--audit] [--bin-links] [--fund] [--dry-run]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: ddp

Run "npm help dedupe" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command deprecate > must match snapshot 1`] = `
npm deprecate

Deprecate a version of a package

Usage:
npm deprecate <pkg>[@<version>] <message>

Options:
[--registry <registry>] [--otp <otp>]

Run "npm help deprecate" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command diff > must match snapshot 1`] = `
npm diff

The registry diff command

Usage:
npm diff [...<paths>]

Options:
[--diff <pkg-name|spec|version> [--diff <pkg-name|spec|version> ...]]
[--diff-name-only] [--diff-unified <number>] [--diff-ignore-all-space]
[--diff-no-prefix] [--diff-src-prefix <path>] [--diff-dst-prefix <path>]
[--diff-text] [-g|--global] [--tag <tag>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

Run "npm help diff" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command dist-tag > must match snapshot 1`] = `
npm dist-tag

Modify package distribution tags

Usage:
npm dist-tag add <pkg>@<version> [<tag>]
npm dist-tag rm <pkg> <tag>
npm dist-tag ls [<pkg>]

Options:
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: dist-tags

Run "npm help dist-tag" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command docs > must match snapshot 1`] = `
npm docs

Open documentation for a package in a web browser

Usage:
npm docs [<pkgname> [<pkgname> ...]]

Options:
[--browser|--browser <browser>] [--registry <registry>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: home

Run "npm help docs" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command doctor > must match snapshot 1`] = `
npm doctor

Check your npm environment

Usage:
npm doctor

Options:
[--registry <registry>]

Run "npm help doctor" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command edit > must match snapshot 1`] = `
npm edit

Edit an installed package

Usage:
npm edit <pkg>[/<subpkg>...]

Options:
[--editor <editor>]

Run "npm help edit" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command exec > must match snapshot 1`] = `
npm exec

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
[-ws|--workspaces]

alias: x

Run "npm help exec" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command explain > must match snapshot 1`] = `
npm explain

Explain installed packages

Usage:
npm explain <folder | specifier>

Options:
[--json] [-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]

alias: why

Run "npm help explain" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command explore > must match snapshot 1`] = `
npm explore

Browse an installed package

Usage:
npm explore <pkg> [ -- <command>]

Options:
[--shell <shell>]

Run "npm help explore" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command find-dupes > must match snapshot 1`] = `
npm find-dupes

Find duplication in the package tree

Usage:
npm find-dupes

Options:
[--global-style] [--legacy-bundling] [--strict-peer-deps] [--package-lock]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--ignore-scripts]
[--audit] [--bin-links] [--fund]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

Run "npm help find-dupes" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command fund > must match snapshot 1`] = `
npm fund

Retrieve funding information

Usage:
npm fund [[<@scope>/]<pkg>]

Options:
[--json] [--browser|--browser <browser>] [--unicode]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[--which <fundingSourceNumber>]

Run "npm help fund" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command get > must match snapshot 1`] = `
npm get

Get a value from the npm configuration

Usage:
npm get [<key> ...] (See \`npm config\`)

Run "npm help get" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command help > must match snapshot 1`] = `
npm help

Get help on npm

Usage:
npm help <term> [<terms..>]

Options:
[--viewer <viewer>]

alias: hlep

Run "npm help help" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command hook > must match snapshot 1`] = `
npm hook

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
npm init

Create a package.json file

Usage:
npm init [--force|-f|--yes|-y|--scope]
npm init <@scope> (same as \`npx <@scope>/create\`)
npm init [<@scope>/]<name> (same as \`npx [<@scope>/]create-<name>\`)

Options:
[-y|--yes] [-f|--force]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

aliases: create, innit

Run "npm help init" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command install > must match snapshot 1`] = `
npm install

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
[-S|--save|--no-save|--save-prod|--save-dev|--save-optional|--save-peer]
[-E|--save-exact] [-g|--global] [--global-style] [--legacy-bundling]
[--strict-peer-deps] [--package-lock]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--ignore-scripts]
[--audit] [--bin-links] [--fund] [--dry-run]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

aliases: i, in, ins, inst, insta, instal, isnt, isnta, isntal, add

Run "npm help install" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command install-ci-test > must match snapshot 1`] = `
npm install-ci-test

Install a project with a clean slate and run tests

Usage:
npm install-ci-test

Options:
[--ignore-scripts] [--script-shell <script-shell>]

alias: cit

Run "npm help install-ci-test" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command install-test > must match snapshot 1`] = `
npm install-test

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
[-S|--save|--no-save|--save-prod|--save-dev|--save-optional|--save-peer]
[-E|--save-exact] [-g|--global] [--global-style] [--legacy-bundling]
[--strict-peer-deps] [--package-lock]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--ignore-scripts]
[--audit] [--bin-links] [--fund] [--dry-run]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: it

Run "npm help install-test" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command link > must match snapshot 1`] = `
npm link

Symlink a package folder

Usage:
npm link (in package dir)
npm link [<@scope>/]<pkg>[@<version>]

Options:
[-S|--save|--no-save|--save-prod|--save-dev|--save-optional|--save-peer]
[-E|--save-exact] [-g|--global] [--global-style] [--legacy-bundling]
[--strict-peer-deps] [--package-lock]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--ignore-scripts]
[--audit] [--bin-links] [--fund] [--dry-run]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: ln

Run "npm help link" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command ll > must match snapshot 1`] = `
npm ll

List installed packages

Usage:
npm ll [[<@scope>/]<pkg> ...]

Options:
[-a|--all] [--json] [-l|--long] [-p|--parseable] [-g|--global] [--depth <depth>]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--link]
[--package-lock-only] [--unicode]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: la

Run "npm help ll" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command login > must match snapshot 1`] = `
npm adduser

Add a registry user account

Usage:
npm adduser

Options:
[--registry <registry>] [--scope <@scope>]

aliases: login, add-user

Run "npm help adduser" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command logout > must match snapshot 1`] = `
npm logout

Log out of the registry

Usage:
npm logout

Options:
[--registry <registry>] [--scope <@scope>]

Run "npm help logout" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command ls > must match snapshot 1`] = `
npm ls

List installed packages

Usage:
npm ls [[<@scope>/]<pkg> ...]

Options:
[-a|--all] [--json] [-l|--long] [-p|--parseable] [-g|--global] [--depth <depth>]
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--link]
[--package-lock-only] [--unicode]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: list

Run "npm help ls" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command org > must match snapshot 1`] = `
npm org

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
npm outdated

Check for outdated packages

Usage:
npm outdated [[<@scope>/]<pkg> ...]

Options:
[-a|--all] [--json] [-l|--long] [-p|--parseable] [-g|--global]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]

Run "npm help outdated" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command owner > must match snapshot 1`] = `
npm owner

Manage package owners

Usage:
npm owner add <user> [<@scope>/]<pkg>
npm owner rm <user> [<@scope>/]<pkg>
npm owner ls [<@scope>/]<pkg>

Options:
[--registry <registry>] [--otp <otp>]

alias: author

Run "npm help owner" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command pack > must match snapshot 1`] = `
npm pack

Create a tarball from a package

Usage:
npm pack [[<@scope>/]<pkg>...]

Options:
[--dry-run] [--json] [--pack-destination <pack-destination>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

Run "npm help pack" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command ping > must match snapshot 1`] = `
npm ping

Ping npm registry

Usage:
npm ping

Options:
[--registry <registry>]

Run "npm help ping" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command prefix > must match snapshot 1`] = `
npm prefix

Display prefix

Usage:
npm prefix [-g]

Options:
[-g|--global]

Run "npm help prefix" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command profile > must match snapshot 1`] = `
npm profile

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
npm prune

Remove extraneous packages

Usage:
npm prune [[<@scope>/]<pkg>...]

Options:
[--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]] [--dry-run]
[--json] [-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

Run "npm help prune" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command publish > must match snapshot 1`] = `
npm publish

Publish a package

Usage:
npm publish [<folder>]

Options:
[--tag <tag>] [--access <restricted|public>] [--dry-run] [--otp <otp>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

Run "npm help publish" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command rebuild > must match snapshot 1`] = `
npm rebuild

Rebuild a package

Usage:
npm rebuild [[<@scope>/]<name>[@<version>] ...]

Options:
[-g|--global] [--bin-links] [--ignore-scripts]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: rb

Run "npm help rebuild" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command repo > must match snapshot 1`] = `
npm repo

Open package repository page in the browser

Usage:
npm repo [<pkgname> [<pkgname> ...]]

Options:
[--browser|--browser <browser>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

Run "npm help repo" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command restart > must match snapshot 1`] = `
npm restart

Restart a package

Usage:
npm restart [-- <args>]

Options:
[--ignore-scripts] [--script-shell <script-shell>]

Run "npm help restart" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command root > must match snapshot 1`] = `
npm root

Display npm root

Usage:
npm root

Options:
[-g|--global]

Run "npm help root" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command run-script > must match snapshot 1`] = `
npm run-script

Run arbitrary package scripts

Usage:
npm run-script <command> [-- <args>]

Options:
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces] [--if-present] [--ignore-scripts]
[--script-shell <script-shell>]

aliases: run, rum, urn

Run "npm help run-script" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command search > must match snapshot 1`] = `
npm search

Search for packages

Usage:
npm search [search terms ...]

Options:
[-l|--long] [--json] [--color|--color <always>] [-p|--parseable]
[--no-description] [--searchopts <searchopts>] [--searchexclude <searchexclude>]
[--registry <registry>] [--prefer-online] [--prefer-offline] [--offline]

aliases: s, se, find

Run "npm help search" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command set > must match snapshot 1`] = `
npm set

Set a value in the npm configuration

Usage:
npm set <key>=<value> [<key>=<value> ...] (See \`npm config\`)

Run "npm help set" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command set-script > must match snapshot 1`] = `
npm set-script

Set tasks in the scripts section of package.json

Usage:
npm set-script [<script>] [<command>]

Options:
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

Run "npm help set-script" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command shrinkwrap > must match snapshot 1`] = `
npm shrinkwrap

Lock down dependency versions for publication

Usage:
npm shrinkwrap

Run "npm help shrinkwrap" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command star > must match snapshot 1`] = `
npm star

Mark your favorite packages

Usage:
npm star [<pkg>...]

Options:
[--registry <registry>] [--unicode]

Run "npm help star" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command stars > must match snapshot 1`] = `
npm stars

View packages marked as favorites

Usage:
npm stars [<user>]

Options:
[--registry <registry>]

Run "npm help stars" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command start > must match snapshot 1`] = `
npm start

Start a package

Usage:
npm start [-- <args>]

Options:
[--ignore-scripts] [--script-shell <script-shell>]

Run "npm help start" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command stop > must match snapshot 1`] = `
npm stop

Stop a package

Usage:
npm stop [-- <args>]

Options:
[--ignore-scripts] [--script-shell <script-shell>]

Run "npm help stop" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command team > must match snapshot 1`] = `
npm team

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
npm test

Test a package

Usage:
npm test [-- <args>]

Options:
[--ignore-scripts] [--script-shell <script-shell>]

aliases: tst, t

Run "npm help test" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command token > must match snapshot 1`] = `
npm token

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
npm uninstall

Remove a package

Usage:
npm uninstall [<@scope>/]<pkg>...

Options:
[-S|--save|--no-save|--save-prod|--save-dev|--save-optional|--save-peer]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

aliases: un, unlink, remove, rm, r

Run "npm help uninstall" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command unpublish > must match snapshot 1`] = `
npm unpublish

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
npm unstar

Remove an item from your favorite packages

Usage:
npm unstar [<pkg>...]

Options:
[--registry <registry>] [--unicode] [--otp <otp>]

Run "npm help unstar" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command update > must match snapshot 1`] = `
npm update

Update packages

Usage:
npm update [<pkg>...]

Options:
[-g|--global] [--global-style] [--legacy-bundling] [--strict-peer-deps]
[--package-lock] [--omit <dev|optional|peer> [--omit <dev|optional|peer> ...]]
[--ignore-scripts] [--audit] [--bin-links] [--fund] [--dry-run]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

aliases: up, upgrade, udpate

Run "npm help update" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command version > must match snapshot 1`] = `
npm version

Bump a package version

Usage:
npm version [<newversion> | major | minor | patch | premajor | preminor | prepatch | prerelease | from-git]

Options:
[--allow-same-version] [--commit-hooks] [--git-tag-version] [--json]
[--preid prerelease-id] [--sign-git-tag]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: verison

Run "npm help version" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command view > must match snapshot 1`] = `
npm view

View registry info

Usage:
npm view [<@scope>/]<pkg>[@<version>] [<field>[.subfield]...]

Options:
[--json] [-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

aliases: v, info, show

Run "npm help view" for more info
`

exports[`test/lib/load-all-commands.js TAP load each command whoami > must match snapshot 1`] = `
npm whoami

Display npm username

Usage:
npm whoami

Options:
[--registry <registry>]

Run "npm help whoami" for more info
`
