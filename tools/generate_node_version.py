#!/usr/bin/env python3

import sys


def main(input_file, output_file, node_module_version, napi_build_version):
  with open(input_file, 'r') as i:
    content = i.read() \
               .replace("/*NODE_MODULE_VERSION_PLACEHOLDER*/", \
                        node_module_version) \
               .replace("/*NAPI_BUILD_VERSION_PLACEHOLDER*/", \
                        napi_build_version)

    with open(output_file, 'w') as o:
      o.write(content)


if __name__ == '__main__':
  if len(sys.argv) != 5:
    print("Usage: generate_node_version.py "
          "<node_version.h.in> <node_version.h> "
          "<node_module_version> <napi_build_version>")
    sys.exit(1)

  main(*sys.argv[1:])
