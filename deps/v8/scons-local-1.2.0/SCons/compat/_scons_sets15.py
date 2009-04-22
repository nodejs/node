#
# A Set class that works all the way back to Python 1.5.  From:
#
#       Python Cookbook:  Yet another Set class for Python
#       http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/106469
#       Goncalo Rodriques
#
#       This is a pure Pythonic implementation of a set class.  The syntax
#       and methods implemented are, for the most part, borrowed from
#       PEP 218 by Greg Wilson.
#
# Note that this class violates the formal definition of a set() by adding
# a __getitem__() method so we can iterate over a set's elements under
# Python 1.5 and 2.1, which don't support __iter__() and iterator types.
#

import string

class Set:
    """The set class. It can contain mutable objects."""

    def __init__(self, seq = None):
        """The constructor. It can take any object giving an iterator as an optional
        argument to populate the new set."""
        self.elems = []
        if seq:
            for elem in seq:
                if elem not in self.elems:
                    hash(elem)
                    self.elems.append(elem)

    def __str__(self):
        return "set([%s])" % string.join(map(str, self.elems), ", ")


    def copy(self):
        """Shallow copy of a set object."""
        return Set(self.elems)

    def __contains__(self, elem):
        return elem in self.elems

    def __len__(self):
        return len(self.elems)

    def __getitem__(self, index):
        # Added so that Python 1.5 can iterate over the elements.
        # The cookbook recipe's author didn't like this because there
        # really isn't any order in a set object, but this is necessary
        # to make the class work well enough for our purposes.
        return self.elems[index]

    def items(self):
        """Returns a list of the elements in the set."""
        return self.elems

    def add(self, elem):
        """Add one element to the set."""
        if elem not in self.elems:
            hash(elem)
            self.elems.append(elem)

    def remove(self, elem):
        """Remove an element from the set. Return an error if elem is not in the set."""
        try:
            self.elems.remove(elem)
        except ValueError:
            raise LookupError, "Object %s is not a member of the set." % str(elem)

    def discard(self, elem):
        """Remove an element from the set. Do nothing if elem is not in the set."""
        try:
            self.elems.remove(elem)
        except ValueError:
            pass

    def sort(self, func=cmp):
        self.elems.sort(func)

    #Define an iterator for a set.
    def __iter__(self):
        return iter(self.elems)

    #The basic binary operations with sets.
    def __or__(self, other):
        """Union of two sets."""
        ret = self.copy()
        for elem in other.elems:
            if elem not in ret:
                ret.elems.append(elem)
        return ret

    def __sub__(self, other):
        """Difference of two sets."""
        ret = self.copy()
        for elem in other.elems:
            ret.discard(elem)
        return ret

    def __and__(self, other):
        """Intersection of two sets."""
        ret = Set()
        for elem in self.elems:
            if elem in other.elems:
                ret.elems.append(elem)
        return ret

    def __add__(self, other):
        """Symmetric difference of two sets."""
        ret = Set()
        temp = other.copy()
        for elem in self.elems:
            if elem in temp.elems:
                temp.elems.remove(elem)
            else:
                ret.elems.append(elem)
        #Add remaining elements.
        for elem in temp.elems:
                ret.elems.append(elem)
        return ret

    def __mul__(self, other):
        """Cartesian product of two sets."""
        ret = Set()
        for elemself in self.elems:
            x = map(lambda other, s=elemself: (s, other), other.elems)
            ret.elems.extend(x)
        return ret

    #Some of the binary comparisons.
    def __lt__(self, other):
        """Returns 1 if the lhs set is contained but not equal to the rhs set."""
        if len(self.elems) < len(other.elems):
            temp = other.copy()
            for elem in self.elems:
                if elem in temp.elems:
                    temp.remove(elem)
                else:
                    return 0
            return len(temp.elems) == 0
        else:
            return 0

    def __le__(self, other):
        """Returns 1 if the lhs set is contained in the rhs set."""
        if len(self.elems) <= len(other.elems):
            ret = 1
            for elem in self.elems:
                if elem not in other.elems:
                    ret = 0
                    break
            return ret
        else:
            return 0

    def __eq__(self, other):
        """Returns 1 if the sets are equal."""
        if len(self.elems) != len(other.elems):
            return 0
        else:
            return len(self - other) == 0

    def __cmp__(self, other):
        """Returns 1 if the sets are equal."""
        if self.__lt__(other):
            return -1
        elif other.__lt__(self):
            return 1
        else:
            return 0
