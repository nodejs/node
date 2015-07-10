npm-ping(3) -- Ping npm registry
================================

## SYNOPSIS

    npm.registry.ping(registry, options, function (er, pong))

## DESCRIPTION

Attempts to connect to the given registry, returning a `pong`
object with various metadata if it succeeds.

This function is primarily useful for debugging connection issues
to npm registries.
