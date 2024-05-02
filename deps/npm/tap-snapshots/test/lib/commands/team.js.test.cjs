/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/team.js TAP team add <scope:team> <user> --parseable > should output success result for parseable add user 1`] = `
foo	npmcli:developers	added
`

exports[`test/lib/commands/team.js TAP team add <scope:team> <user> default output > should output success result for add user 1`] = `
foo added to @npmcli:developers
`

exports[`test/lib/commands/team.js TAP team create <scope:team> --parseable > should output parseable success result for create team 1`] = `
npmcli:newteam	created
`

exports[`test/lib/commands/team.js TAP team create <scope:team> default output > should output success result for create team 1`] = `
+@npmcli:newteam
`

exports[`test/lib/commands/team.js TAP team destroy <scope:team> --parseable > should output parseable result for destroy team 1`] = `
npmcli:newteam	deleted
`

exports[`test/lib/commands/team.js TAP team destroy <scope:team> default output > should output success result for destroy team 1`] = `
-@npmcli:newteam
`

exports[`test/lib/commands/team.js TAP team ls <scope:team> --parseable > should list users for a parseable scope:team 1`] = `
darcyclarke
isaacs
nlf
ruyadorno
`

exports[`test/lib/commands/team.js TAP team ls <scope:team> default output > should list users for a given scope:team 1`] = `
@npmcli:developers has 4 users:
darcyclarke
isaacs
nlf
ruyadorno
`

exports[`test/lib/commands/team.js TAP team ls <scope:team> no users > should list no users for a given scope 1`] = `
@npmcli:developers has 0 users
`

exports[`test/lib/commands/team.js TAP team ls <scope:team> single user > should list single user for a given scope 1`] = `
@npmcli:developers has 1 user:
foo
`

exports[`test/lib/commands/team.js TAP team ls <scope> --parseable > should list teams for a parseable scope 1`] = `
npmcli:designers
npmcli:developers
npmcli:product
`

exports[`test/lib/commands/team.js TAP team ls <scope> default output > should list teams for a given scope 1`] = `
@npmcli has 3 teams:
@npmcli:designers
@npmcli:developers
@npmcli:product
`

exports[`test/lib/commands/team.js TAP team ls <scope> no teams > should list no teams for a given scope 1`] = `
@npmcli has 0 teams
`

exports[`test/lib/commands/team.js TAP team ls <scope> single team > should list single team for a given scope 1`] = `
@npmcli has 1 team:
@npmcli:developers
`

exports[`test/lib/commands/team.js TAP team rm <scope:team> <user> --parseable > should output parseable result for remove user 1`] = `
foo	npmcli:newteam	removed
`

exports[`test/lib/commands/team.js TAP team rm <scope:team> <user> default output > should output success result for remove user 1`] = `
foo removed from @npmcli:newteam
`
