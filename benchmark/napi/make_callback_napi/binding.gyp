{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'binding.c' ],
      'conditions': [
        # node-gyp does not pass the include dirs of the Node.js build, and
        # the addon headers have uv.h only when libuv is bundled.
        [ 'node_shared_libuv=="true"', {
          'include_dirs': [ '<@(node_shared_libuv_include_dirs)' ],
        }],
      ],
    }
  ]
}
