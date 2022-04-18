/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/profile.js TAP enable-2fa from token and set otp, retries on pending and verifies with qrcode > should output 2fa enablement success msgs 1`] = `
Scan into your authenticator app:
qrcode
 Or enter code:
1234
2FA successfully enabled. Below are your recovery codes, please print these out.
You will need these to recover access to your account if you lose your authentication device.
	123456
	789101
`

exports[`test/lib/commands/profile.js TAP profile get <key> --parseable > should output parseable result value 1`] = `
foo
`

exports[`test/lib/commands/profile.js TAP profile get multiple args --parseable > should output parseable profile value results 1`] = `
foo	foo@github.com (verified)	https://github.com/npm
`

exports[`test/lib/commands/profile.js TAP profile get multiple args comma separated > should output all keys 1`] = `
foo	foo@github.com (verified)	https://github.com/npm
`

exports[`test/lib/commands/profile.js TAP profile get multiple args default output > should output all keys 1`] = `
foo	foo@github.com (verified)	https://github.com/npm
`

exports[`test/lib/commands/profile.js TAP profile get no args --parseable > should output all profile info as parseable result 1`] = `
tfa	auth-and-writes
name	foo
email	foo@github.com
email_verified	true
created	2015-02-26T01:26:37.384Z
updated	2020-08-12T16:19:35.326Z
fullname	Foo Bar
homepage	https://github.com
freenode	foobar
twitter	https://twitter.com/npmjs
github	https://github.com/npm
`

exports[`test/lib/commands/profile.js TAP profile get no args default output > should output table with contents 1`] = `
[1mname[22m: foo
[1memail[22m: foo@github.com (verified)
[1mtwo-factor auth[22m: auth-and-writes
[1mfullname[22m: Foo Bar
[1mhomepage[22m: https://github.com
[1mfreenode[22m: foobar
[1mtwitter[22m: https://twitter.com/npmjs
[1mgithub[22m: https://github.com/npm
[1mcreated[22m: 2015-02-26T01:26:37.384Z
[1mupdated[22m: 2020-08-12T16:19:35.326Z
`

exports[`test/lib/commands/profile.js TAP profile get no args no tfa enabled > should output expected profile values 1`] = `
[1mname[22m: foo
[1memail[22m: foo@github.com (verified)
[1mtwo-factor auth[22m: disabled
[1mfullname[22m: Foo Bar
[1mhomepage[22m: https://github.com
[1mfreenode[22m: foobar
[1mtwitter[22m: https://twitter.com/npmjs
[1mgithub[22m: https://github.com/npm
[1mcreated[22m: 2015-02-26T01:26:37.384Z
[1mupdated[22m: 2020-08-12T16:19:35.326Z
`

exports[`test/lib/commands/profile.js TAP profile get no args profile has cidr_whitelist item > should output table with contents 1`] = `
[1mname[22m: foo
[1memail[22m: foo@github.com (verified)
[1mtwo-factor auth[22m: auth-and-writes
[1mfullname[22m: Foo Bar
[1mhomepage[22m: https://github.com
[1mfreenode[22m: foobar
[1mtwitter[22m: https://twitter.com/npmjs
[1mgithub[22m: https://github.com/npm
[1mcreated[22m: 2015-02-26T01:26:37.384Z
[1mupdated[22m: 2020-08-12T16:19:35.326Z
[1mcidr_whitelist[22m: 192.168.1.1
`

exports[`test/lib/commands/profile.js TAP profile get no args unverified email > should output table with contents 1`] = `
[1mname[22m: foo
[1memail[22m: foo@github.com(unverified)
[1mtwo-factor auth[22m: auth-and-writes
[1mfullname[22m: Foo Bar
[1mhomepage[22m: https://github.com
[1mfreenode[22m: foobar
[1mtwitter[22m: https://twitter.com/npmjs
[1mgithub[22m: https://github.com/npm
[1mcreated[22m: 2015-02-26T01:26:37.384Z
[1mupdated[22m: 2020-08-12T16:19:35.326Z
`

exports[`test/lib/commands/profile.js TAP profile set <key> <value> writable key --parseable > should output parseable set key success msg 1`] = `
fullname	Lorem Ipsum
`
