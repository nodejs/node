window.checkErrorArguments = args => {
  assert_equals(args.length, 1);
  assert_equals(args[0].constructor, Event);
};
