// getraises and setraises are not longer valid Web IDL
interface Person {

  // An attribute that can raise an exception if it is set to an invalid value.
  attribute DOMString name setraises (InvalidName);

  // An attribute whose value cannot be assigned to, and which can raise an
  // exception some circumstances.
  readonly attribute DOMString petName getraises (NoSuchPet);
};

exception SomeException {
};

interface ExceptionThrower {
  // This attribute always throws a SomeException and never returns a value.
  attribute long valueOf getraises(SomeException);
};