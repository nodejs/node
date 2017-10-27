npm-profile(1) -- Change settings on your registry profile
==========================================================

## SYNOPSIS

    npm profile get [--json|--parseable] [<property>]
    npm profile set [--json|--parseable] <property> <value>
    npm profile set password
    npm profile enable-2fa [auth-and-writes|auth-only]
    npm profile disable-2fa

## DESCRIPTION

Change your profile information on the registry.  This not be available if
you're using a non-npmjs registry.

* `npm profile get [<property>]`:
  Display all of the properties of your profile, or one or more specific
  properties.  It looks like:

```
+-----------------+---------------------------+
| name            | example                   |
+-----------------+---------------------------+
| email           | me@example.com (verified) |
+-----------------+---------------------------+
| two factor auth | auth-and-writes           |
+-----------------+---------------------------+
| fullname        | Example User              |
+-----------------+---------------------------+
| homepage        |                           |
+-----------------+---------------------------+
| freenode        |                           |
+-----------------+---------------------------+
| twitter         |                           |
+-----------------+---------------------------+
| github          |                           |
+-----------------+---------------------------+
| created         | 2015-02-26T01:38:35.892Z  |
+-----------------+---------------------------+
| updated         | 2017-10-02T21:29:45.922Z  |
+-----------------+---------------------------+
```

* `npm profile set <property> <value>`:
  Set the value of a profile property. You can set the following properties this way:
    email, fullname, homepage, freenode, twitter, github

* `npm profile set password`:
  Change your password.  This is interactive, you'll be prompted for your
  current password and a new password.  You'll also be prompted for an OTP
  if you have two-factor authentication enabled.

* `npm profile enable-2fa [auth-and-writes|auth-only]`:
  Enables two-factor authentication. Defaults to `auth-and-writes` mode. Modes are:
  * `auth-only`: Require an OTP when logging in or making changes to your
    account's authentication.  The OTP will be required on both the website
    and the command line.
  * `auth-and-writes`: Requires an OTP at all the times `auth-only` does, and also requires one when
    publishing a module, setting the `latest` dist-tag, or changing access
    via `npm access` and `npm owner`.

* `npm profile disable-2fa`:
  Disables two-factor authentication.

## DETAILS

All of the `npm profile` subcommands accept `--json` and `--parseable` and
will tailor their output based on those.  Some of these commands may not be
available on non npmjs.com registries.

## SEE ALSO

* npm-config(7)
