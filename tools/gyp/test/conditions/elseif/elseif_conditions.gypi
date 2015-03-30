{
  'conditions': [
    ['test_var==0', {
      'defines': ['FOO="first_if"'],
    }, 'test_var==1', {
      'defines': ['FOO="first_else_if"'],
    }, 'test_var==2', {
      'defines': ['FOO="second_else_if"'],
    }, 'test_var==3', {
      'defines': ['FOO="third_else_if"'],
    }, {
      'defines': ['FOO="last_else"'],
    }],
  ],
}
