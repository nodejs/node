# This file comes from
#   https://github.com/martine/ninja/blob/master/misc/ninja_syntax.py
# Do not edit!  Edit the upstream one instead.

"""Python module for generating .ninja files.

Note that this is emphatically not a required piece of Ninja; it's
just a helpful utility for build-file-generation systems that already
use Python.
"""

import textwrap
import re

def escape_spaces(word):
    return word.replace('$ ','$$ ').replace(' ','$ ')

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
        if value is None:
            return
        if isinstance(value, list):
            value = ' '.join(value)
        self._line('%s = %s' % (key, value), indent)

    def rule(self, name, command, description=None, depfile=None,
             generator=False):
        self._line('rule %s' % name)
        self.variable('command', command, indent=1)
        if description:
            self.variable('description', description, indent=1)
        if depfile:
            self.variable('depfile', depfile, indent=1)
        if generator:
            self.variable('generator', '1', indent=1)

    def build(self, outputs, rule, inputs=None, implicit=None, order_only=None,
              variables=None):
        outputs = self._as_list(outputs)
        all_inputs = self._as_list(inputs)[:]
        out_outputs = map(escape_spaces, outputs)
        all_inputs = map(escape_spaces, all_inputs)

        if implicit:
            implicit = map(escape_spaces, self._as_list(implicit))
            all_inputs.append('|')
            all_inputs.extend(implicit)
        if order_only:
            order_only = map(escape_spaces, self._as_list(order_only))
            all_inputs.append('||')
            all_inputs.extend(order_only)

        self._line('build %s: %s %s' % (' '.join(out_outputs),
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

    def default(self, paths):
        self._line('default %s' % ' '.join(self._as_list(paths)))

    def _line(self, text, indent=0):
        """Write 'text' word-wrapped at self.width characters."""
        leading_space = '  ' * indent
        while len(text) > self.width:
            # The text is too wide; wrap if possible.

            self.output.write(leading_space)

            available_space = self.width - len(leading_space) - len(' $')

            # Write as much as we can into this line.
            done = False
            written_stuff = False
            while available_space > 0:
                space = re.search('((\$\$)+([^$]|^)|[^$]|^) ', text)
                if space:
                    space_idx = space.start() + 1
                else:
                    # No spaces left.
                    done = True
                    break

                if space_idx > available_space:
                    # We're out of space.
                    if written_stuff:
                        # See if we can fit it on the next line.
                        break
                    # If we haven't written anything yet on this line, don't
                    # try to wrap.
                self.output.write(text[0:space_idx] + ' ')
                written_stuff = True
                text = text[space_idx+1:]
                available_space -= space_idx+1

            self.output.write('$\n')

            # Subsequent lines are continuations, so indent them.
            leading_space = '  ' * (indent+2)

            if done:
                # No more spaces, so bail.
                break

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
