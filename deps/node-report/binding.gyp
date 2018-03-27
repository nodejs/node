{
  "targets": [
    {
      "target_name": "api",
      "sources": [ "src/node_report.cc", "src/module.cc", "src/utilities.cc" ],
      "include_dirs": [ '<!(node -e "require(\'nan\')")' ],
      "conditions": [
        ["OS=='linux'", {
          "defines": [ "_GNU_SOURCE" ],
          "cflags": [ "-g", "-O2", "-std=c++11", ],
        }],
        ["OS=='win'", {
          "libraries": [ "dbghelp.lib", "Netapi32.lib", "PsApi.lib", "Ws2_32.lib" ],
          "dll_files": [ "dbghelp.dll", "Netapi32.dll", "PsApi.dll", "Ws2_32.dll" ],
        }],
      ],
      "defines": [
        'NODEREPORT_VERSION="<!(node -p \"require(\'./package.json\').version\")"'
      ],
    },
    {
      "target_name": "install",
      "type":"none",
      "dependencies" : [ "api" ],
      "copies": [
        {
          "destination": "<(module_root_dir)",
          "files": ["<(PRODUCT_DIR)/api.node"]
        }]
    },
  ],
}

