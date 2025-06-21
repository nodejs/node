This is a reference implementation of the ITT API _dynamic part_
that performs tracing data from ITT API functions calls to log files.

To use this solution it is requered to build it like a shared library and add
full library path to the `INTEL_LIBITTNOTIFY64/INTEL_LIBITTNOTIFY32` environment variable:
```
make
export INTEL_LIBITTNOTIFY64=<build_dir>/libittnotify_refcol.so
```

Temp directory is used by default to save log files.
To change log directory use the `INTEL_LIBITTNOTIFY_LOG_DIR` environment variable:
```
export INTEL_LIBITTNOTIFY_LOG_DIR=<log_dir>
```

This implementation adds logging of some of the ITT API functions calls.
Adding logging of the other ITT API functions calls are welcome.
The solution provides 4 functions with different log levels
that takes printf format for logging:
```
LOG_FUNC_CALL_INFO(const char *msg_format, ...);
LOG_FUNC_CALL_WARN(const char *msg_format, ...);
LOG_FUNC_CALL_ERROR(const char *msg_format, ...);
LOG_FUNC_CALL_FATAL(const char *msg_format, ...);
```
