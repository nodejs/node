npm-ping(1) -- Ping npm registry
================================

## SYNOPSIS

    npm ping [--registry <registry>]

## DESCRIPTION

Ping the configured or given npm registry and verify authentication.
If it works it will output something like:
```
Ping success: {*Details about registry*}
```
otherwise you will get:
```
Ping error: {*Detail about error}
```

## SEE ALSO

* npm-config(1)
* npm-config(7)
* npmrc(5)
