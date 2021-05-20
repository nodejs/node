/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/config.js TAP config edit --global > should write global config file 1`] = `
;;;;
; npm globalconfig file: /etc/npmrc
; this is a simple ini-formatted file
; lines that start with semi-colons are comments
; run \`npm help 7 config\` for documentation of the various options
;
; Configs like \`@scope:registry\` map a scope to a given registry url.
;
; Configs like \`//<hostname>/:_authToken\` are auth that is restricted
; to the registry host specified.

init.author.name=Foo

;;;;
; all available options shown below with default values
;;;;


; init-author-name=
; init-version=1.0.0
; init.author.name=
; init.version=1.0.0

`

exports[`test/lib/config.js TAP config edit > should write config file 1`] = `
;;;;
; npm userconfig file: ~/.npmrc
; this is a simple ini-formatted file
; lines that start with semi-colons are comments
; run \`npm help 7 config\` for documentation of the various options
;
; Configs like \`@scope:registry\` map a scope to a given registry url.
;
; Configs like \`//<hostname>/:_authToken\` are auth that is restricted
; to the registry host specified.

//registry.npmjs.org/:_authToken=0000000
init.author.name=Foo
sign-git-commit=true

;;;;
; all available options shown below with default values
;;;;


; init-author-name=
; init-version=1.0.0
; init.author.name=
; init.version=1.0.0

`

exports[`test/lib/config.js TAP config edit > should write config file 2`] = `
;;;;
; npm userconfig file: ~/.npmrc
; this is a simple ini-formatted file
; lines that start with semi-colons are comments
; run \`npm help 7 config\` for documentation of the various options
;
; Configs like \`@scope:registry\` map a scope to a given registry url.
;
; Configs like \`//<hostname>/:_authToken\` are auth that is restricted
; to the registry host specified.



;;;;
; all available options shown below with default values
;;;;


; init-author-name=
; init-version=1.0.0
; init.author.name=
; init.version=1.0.0

`

exports[`test/lib/config.js TAP config get no args > should list configs on config get no args 1`] = `
; "cli" config from command line options

cat = true 
chai = true 
dog = true 
editor = "vi" 
global = false 
json = false 
long = false 

; node bin location = /path/to/node
; cwd = {CWD}
; HOME = ~/
; Run \`npm config ls -l\` to show all defaults.
`

exports[`test/lib/config.js TAP config list --long > should list all configs 1`] = `
; "default" config from default values

init-author-name = "" 
init-version = "1.0.0" 
init.author.name = "" 
init.version = "1.0.0" 

; "cli" config from command line options

cat = true 
chai = true 
dog = true 
editor = "vi" 
global = false 
json = false 
long = true
`

exports[`test/lib/config.js TAP config list > should list configs 1`] = `
; "cli" config from command line options

cat = true 
chai = true 
dog = true 
editor = "vi" 
global = false 
json = false 
long = false 

; node bin location = /path/to/node
; cwd = {CWD}
; HOME = ~/
; Run \`npm config ls -l\` to show all defaults.
`

exports[`test/lib/config.js TAP config list overrides > should list overridden configs 1`] = `
; "cli" config from command line options

cat = true 
chai = true 
dog = true 
editor = "vi" 
global = false 
init.author.name = "Bar" 
json = false 
long = false 

; "user" config from ~/.npmrc

; //private-reg.npmjs.org/:_authThoken = (protected) ; overridden by cli
; init.author.name = "Foo" ; overridden by cli

; node bin location = /path/to/node
; cwd = {CWD}
; HOME = ~/
; Run \`npm config ls -l\` to show all defaults.
`
