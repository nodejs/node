#!/usr/bin/python

# This file comes from
#   https://github.com/martine/ninja/blob/master/misc/ninja_syntax.py
# Do not edit!  Edit the upstream one instead.

"""Python module for generating .ninja files.

Note that this is emphatically not a required piece of Ninja; it's
just a helpful utility for build-file-generation systems that already
use Python.
"""

import textwrap

class Writer(object):
    def __init__(self, output, width=78):
        self.output = output
        self.width = width

    def newline(self):
        self.output.write('\n')

    def comment(self, text):
        for line in textwrap.wrap(text, self.width - 2):
            self.output.write('# ' + line + '\n')

    def variable(self, key, value, indent=0):
        self._line('%s = %s' % (key, value), indent)

    def rule(self, name, command, description=None, depfile=None):
        self._line('rule %s' % name)
        self.variable('command', command, indent=1)
        if description:
            self.variable('description', description, indent=1)
        if depfile:
            self.variable('depfile', depfile, indent=1)

    def build(self, outputs, rule, inputs=None, implicit=None, order_only=None,
              variables=None):
        outputs = self._as_list(outputs)
        all_inputs = self._as_list(inputs)[:]

        if implicit:
            all_inputs.append('|')
            all_inputs.extend(self._as_list(implicit))
        if order_only:
            all_inputs.append('||')
            all_inputs.extend(self._as_list(order_only))

        self._line('build %s: %s %s' % (' '.join(outputs),
                                        rule,
                                        ' '.join(all_inputs)))

        if variables:
            for key, val in variables:
                self.variable(key, val, indent=1)

        return outputs

    def include(self, path):
        self._line('include %s' % path)

    def subninja(self, path):
        self._line('subninja %s' % path)

    def _line(self, text, indent=0):
        """Write 'text' word-wrapped at self.width characters."""
        leading_space = '  ' * indent
        while len(text) > self.width:
            # The text is too wide; wrap if possible.

            # Find the rightmost space that would obey our width constraint.
            available_space = self.width - len(leading_space) - len(' $')
            space = text.rfind(' ', 0, available_space)
            if space < 0:
                # No such space; just use the first space we can find.
                space = text.find(' ', available_space)
            if space < 0:
                # Give up on breaking.
                break

            self.output.write(leading_space + text[0:space] + ' $\n')
            text = text[space+1:]

            # Subsequent lines are continuations, so indent them.
            leading_space = '  ' * (indent+2)

        self.output.write(leading_space + text + '\n')

    def _as_list(self, input):
        if input is None:
            return []
        if isinstance(input, list):
            return input
        return [input]


def escape(string):
    """Escape a string such that it can be embedded into a Ninja file without
    further interpretation."""
    assert '\n' not in string, 'Ninja syntax does not allow newlines'
    # We only have one special metacharacter: '$'.
    return string.replace('$', '$$')
