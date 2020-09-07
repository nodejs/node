# Communication model of fuzzilli with V8

## Source code

On low level fuzzilli communicates with v8 through Swift C API library in `Sources/libreprl/libreprl.c`

`reprl_spawn_child` fucntions spawns child process. It does that by creating pipes, forking itself, then setting filedescriptors, and then transforming itself using `execve` into v8 process. Afterwords it checks for receiving 4 byte string and it sends the exact same string back.

`fetch_output` fetches the output from the child and returns its size and pointer to data.

`execute script`
writes `exec`, and size of script, into the command write pipe and sends script through data write pipe

## Coverage

Coverage information are being monitored through shared memory. On the side of v8 it is monitored through SanitizerCoverage module of Clang compiler ( https://clang.llvm.org/docs/SanitizerCoverage.html ) Through shared memory information about edges are shared with fuzzilli which implements counter for error and covered branches of the V8 code in Sources/libcoverage/coverage.c
