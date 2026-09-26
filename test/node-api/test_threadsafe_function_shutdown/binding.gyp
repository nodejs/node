{
  "targets": [
    {
      "target_name": "concurrent_calls",
      "sources": ["concurrent_calls.cc"],
      "cflags_cc": ["--std=c++20"],
      'cflags!': [ '-fno-exceptions', '-fno-rtti' ],
      'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
    },
    {
      "target_name": "reentrant_release",
      "sources": ["reentrant_release.c"]
    },
    {
      "target_name": "multi_thread_count_release",
      "sources": ["multi_thread_count_release.c"]
    }
  ]
}
