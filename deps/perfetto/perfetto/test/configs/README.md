# Configs

This directory contains a number of perfetto TraceConfigs in human readable
form for easier testing. Each file is serialized into the protobuf format at
build time.  For example the file `ftrace.cfg` is serialized to
`out/some_gn_config/ftrace.cfg.protobuf`.

## Example

```bash
$ adb push out/some_gn_config/ftrace.cfg.protobuf /data/local/tmp
$ adb push out/some_gn_config/perfetto /data/local/tmp
$ adb shell 'cd /data/local/tmp && ./perfetto -c ftrace_cfg.protobuf -o out.protobuf'
```

