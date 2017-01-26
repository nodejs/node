
class Patch(object):
    """
    Implements a class decorator suitable for patching an existing class with
    a new namespace.

    For example, consider this trivial class (that your code doesn't own):

    >>> class MyClass:
    ...     def __init__(self, param):
    ...         self.param = param
    ...     def bar(self):
    ...         print("orig bar")

    To add attributes to MyClass, you can use Patch:

    >>> @Patch(MyClass)
    ... class JustANamespace:
    ...     def print_param(self):
    ...         print(self.param)
    >>> ob = MyClass('foo')
    >>> ob.print_param()
    foo

    The namespace is assigned None, so there's no mistaking the purpose
    >>> JustANamespace

    The patcher will replace the existing methods:

    >>> @Patch(MyClass)
    ... class SomeNamespace:
    ...     def bar(self):
    ...         print("replaced bar")
    >>> ob = MyClass('foo')
    >>> ob.bar()
    replaced bar

    But it will not replace methods if no_replace is indicated.

    >>> @Patch(MyClass)
    ... class AnotherNamespace:
    ...     @no_replace
    ...     def bar(self):
    ...         print("candy bar")
    >>> ob = MyClass('foo')
    >>> ob.bar()
    replaced bar

    """

    def __init__(self, target):
        self.target = target

    def __call__(self, patches):
        for name, value in vars(patches).items():
            if name in vars(ReferenceEmptyClass):
                continue
            no_replace = getattr(value, '__no_replace', False)
            if no_replace and hasattr(self.target, name):
                continue
            setattr(self.target, name, value)

def no_replace(f):
    """
    Method decorator to indicate that a method definition shall
    silently be ignored if it already exists in the target class.
    """
    f.__no_replace = True
    return f

class ReferenceEmptyClass(object):
    """
    This empty class will serve as a reference for attributes present on
    any class.
    """
