{
  'targets': [
    {
      'target_name': 'foo',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/dest',
          'files': [
            'read-only-file',
          ],
        },
      ],
    },  # target: foo

    {
      'target_name': 'bar',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/dest',
          'files': [
            'subdir/',
          ],
        },
      ],
    },  # target: bar
  ],
}
