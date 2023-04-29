@echo off

REM -- Echo the git commit hash.  If git isn't available for some reason,
REM -- output nothing instead.

git rev-parse HEAD >NUL 2>NUL && (
    git rev-parse HEAD
) || (
    echo none
)

REM -- Set ERRORLEVEL to 0 using this cryptic syntax.
(call )
