npm-token(1) -- Manage your authentication tokens
=================================================

## SYNOPSIS

    npm token list [--json|--parseable]
    npm token create [--read-only] [--cidr=1.1.1.1/24,2.2.2.2/16]
    npm token revoke <id|token>

## DESCRIPTION

This list you list, create and revoke authentication tokens.

* `npm token list`:
  Shows a table of all active authentication tokens. You can request this as
  JSON with `--json` or tab-separated values with `--parseable`.
```
+--------+---------+------------+----------+----------------+
| id     | token   | created    | read-only | CIDR whitelist |
+--------+---------+------------+----------+----------------+
| 7f3134 | 1fa9ba… | 2017-10-02 | yes      |                |
+--------+---------+------------+----------+----------------+
| c03241 | af7aef… | 2017-10-02 | no       | 192.168.0.1/24 |
+--------+---------+------------+----------+----------------+
| e0cf92 | 3a436a… | 2017-10-02 | no       |                |
+--------+---------+------------+----------+----------------+
| 63eb9d | 74ef35… | 2017-09-28 | no       |                |
+--------+---------+------------+----------+----------------+
| 2daaa8 | cbad5f… | 2017-09-26 | no       |                |
+--------+---------+------------+----------+----------------+
| 68c2fe | 127e51… | 2017-09-23 | no       |                |
+--------+---------+------------+----------+----------------+
| 6334e1 | 1dadd1… | 2017-09-23 | no       |                |
+--------+---------+------------+----------+----------------+
```

* `npm token create [--read-only] [--cidr=<cidr-ranges>]`:
  Create a new authentication token. It can be `--read-only` or accept a list of
  [CIDR](https://en.wikipedia.org/wiki/Classless_Inter-Domain_Routing) ranges to
  limit use of this token to. This will prompt you for your password, and, if you have
  two-factor authentication enabled, an otp.

```
+----------------+--------------------------------------+
| token          | a73c9572-f1b9-8983-983d-ba3ac3cc913d |
+----------------+--------------------------------------+
| cidr_whitelist |                                      |
+----------------+--------------------------------------+
| readonly       | false                                |
+----------------+--------------------------------------+
| created        | 2017-10-02T07:52:24.838Z             |
+----------------+--------------------------------------+
```

* `npm token revoke <token|id>`:
  This removes an authentication token, making it immediately unusable. This can accept
  both complete tokens (as you get back from `npm token create` and will
  find in your `.npmrc`) and ids as seen in the `npm token list` output. 
  This will NOT accept the truncated token found in `npm token list` output.
