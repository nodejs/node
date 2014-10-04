{
  "targets": [{
    "target_name": "debugger-agent",
    "type": "<(library)",
    "include_dirs": [
      "src",
      "include",
      "../v8/include",
      "../uv/include",

      # Private node.js folder and stuff needed to include from it
      "../../src",
      "../cares/include",
    ],
    "direct_dependent_settings": {
      "include_dirs": [
        "include",
      ],
    },
    "sources": [
      "src/agent.cc",
    ],
  }],
}
