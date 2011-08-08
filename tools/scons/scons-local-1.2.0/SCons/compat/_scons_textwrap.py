"""Text wrapping and filling.
"""

# Copyright (C) 1999-2001 Gregory P. Ward.
# Copyright (C) 2002, 2003 Python Software Foundation.
# Written by Greg Ward <gward@python.net>

__revision__ = "$Id: textwrap.py,v 1.32.8.2 2004/05/13 01:48:15 gward Exp $"

import string, re

try:
   unicode
except NameError:
   class unicode:
       pass

# Do the right thing with boolean values for all known Python versions
# (so this module can be copied to projects that don't depend on Python
# 2.3, e.g. Optik and Docutils).
try:
    True, False
except NameError:
    (True, False) = (1, 0)

__all__ = ['TextWrapper', 'wrap', 'fill']

# Hardcode the recognized whitespace characters to the US-ASCII
# whitespace characters.  The main reason for doing this is that in
# ISO-8859-1, 0xa0 is non-breaking whitespace, so in certain locales
# that character winds up in string.whitespace.  Respecting
# string.whitespace in those cases would 1) make textwrap treat 0xa0 the
# same as any other whitespace char, which is clearly wrong (it's a
# *non-breaking* space), 2) possibly cause problems with Unicode,
# since 0xa0 is not in range(128).
_whitespace = '\t\n\x0b\x0c\r '

class TextWrapper:
    """
    Object for wrapping/filling text.  The public interface consists of
    the wrap() and fill() methods; the other methods are just there for
    subclasses to override in order to tweak the default behaviour.
    If you want to completely replace the main wrapping algorithm,
    you'll probably have to override _wrap_chunks().

    Several instance attributes control various aspects of wrapping:
      width (default: 70)
        the maximum width of wrapped lines (unless break_long_words
        is false)
      initial_indent (default: "")
        string that will be prepended to the first line of wrapped
        output.  Counts towards the line's width.
      subsequent_indent (default: "")
        string that will be prepended to all lines save the first
        of wrapped output; also counts towards each line's width.
      expand_tabs (default: true)
        Expand tabs in input text to spaces before further processing.
        Each tab will become 1 .. 8 spaces, depending on its position in
        its line.  If false, each tab is treated as a single character.
      replace_whitespace (default: true)
        Replace all whitespace characters in the input text by spaces
        after tab expansion.  Note that if expand_tabs is false and
        replace_whitespace is true, every tab will be converted to a
        single space!
      fix_sentence_endings (default: false)
        Ensure that sentence-ending punctuation is always followed
        by two spaces.  Off by default because the algorithm is
        (unavoidably) imperfect.
      break_long_words (default: true)
        Break words longer than 'width'.  If false, those words will not
        be broken, and some lines might be longer than 'width'.
    """

    whitespace_trans = string.maketrans(_whitespace, ' ' * len(_whitespace))

    unicode_whitespace_trans = {}
    try:
        uspace = eval("ord(u' ')")
    except SyntaxError:
        # Python1.5 doesn't understand u'' syntax, in which case we
        # won't actually use the unicode translation below, so it
        # doesn't matter what value we put in the table.
        uspace = ord(' ')
    for x in map(ord, _whitespace):
        unicode_whitespace_trans[x] = uspace

    # This funky little regex is just the trick for splitting
    # text up into word-wrappable chunks.  E.g.
    #   "Hello there -- you goof-ball, use the -b option!"
    # splits into
    #   Hello/ /there/ /--/ /you/ /goof-/ball,/ /use/ /the/ /-b/ /option!
    # (after stripping out empty strings).
    try:
        wordsep_re = re.compile(r'(\s+|'                  # any whitespace
                                r'[^\s\w]*\w{2,}-(?=\w{2,})|' # hyphenated words
                                r'(?<=[\w\!\"\'\&\.\,\?])-{2,}(?=\w))')   # em-dash
    except re.error:
        # Pre-2.0 Python versions don't have the (?<= negative look-behind
        # assertion.  It mostly doesn't matter for the simple input
        # SCons is going to give it, so just leave it out.
        wordsep_re = re.compile(r'(\s+|'                    # any whitespace
                                r'-*\w{2,}-(?=\w{2,}))')    # hyphenated words

    # XXX will there be a locale-or-charset-aware version of
    # string.lowercase in 2.3?
    sentence_end_re = re.compile(r'[%s]'              # lowercase letter
                                 r'[\.\!\?]'          # sentence-ending punct.
                                 r'[\"\']?'           # optional end-of-quote
                                 % string.lowercase)


    def __init__(self,
                 width=70,
                 initial_indent="",
                 subsequent_indent="",
                 expand_tabs=True,
                 replace_whitespace=True,
                 fix_sentence_endings=False,
                 break_long_words=True):
        self.width = width
        self.initial_indent = initial_indent
        self.subsequent_indent = subsequent_indent
        self.expand_tabs = expand_tabs
        self.replace_whitespace = replace_whitespace
        self.fix_sentence_endings = fix_sentence_endings
        self.break_long_words = break_long_words


    # -- Private methods -----------------------------------------------
    # (possibly useful for subclasses to override)

    def _munge_whitespace(self, text):
        """_munge_whitespace(text : string) -> string

        Munge whitespace in text: expand tabs and convert all other
        whitespace characters to spaces.  Eg. " foo\tbar\n\nbaz"
        becomes " foo    bar  baz".
        """
        if self.expand_tabs:
            text = string.expandtabs(text)
        if self.replace_whitespace:
            if type(text) == type(''):
                text = string.translate(text, self.whitespace_trans)
            elif isinstance(text, unicode):
                text = string.translate(text, self.unicode_whitespace_trans)
        return text


    def _split(self, text):
        """_split(text : string) -> [string]

        Split the text to wrap into indivisible chunks.  Chunks are
        not quite the same as words; see wrap_chunks() for full
        details.  As an example, the text
          Look, goof-ball -- use the -b option!
        breaks into the following chunks:
          'Look,', ' ', 'goof-', 'ball', ' ', '--', ' ',
          'use', ' ', 'the', ' ', '-b', ' ', 'option!'
        """
        chunks = self.wordsep_re.split(text)
        chunks = filter(None, chunks)
        return chunks

    def _fix_sentence_endings(self, chunks):
        """_fix_sentence_endings(chunks : [string])

        Correct for sentence endings buried in 'chunks'.  Eg. when the
        original text contains "... foo.\nBar ...", munge_whitespace()
        and split() will convert that to [..., "foo.", " ", "Bar", ...]
        which has one too few spaces; this method simply changes the one
        space to two.
        """
        i = 0
        pat = self.sentence_end_re
        while i < len(chunks)-1:
            if chunks[i+1] == " " and pat.search(chunks[i]):
                chunks[i+1] = "  "
                i = i + 2
            else:
                i = i + 1

    def _handle_long_word(self, chunks, cur_line, cur_len, width):
        """_handle_long_word(chunks : [string],
                             cur_line : [string],
                             cur_len : int, width : int)

        Handle a chunk of text (most likely a word, not whitespace) that
        is too long to fit in any line.
        """
        space_left = max(width - cur_len, 1)

        # If we're allowed to break long words, then do so: put as much
        # of the next chunk onto the current line as will fit.
        if self.break_long_words:
            cur_line.append(chunks[0][0:space_left])
            chunks[0] = chunks[0][space_left:]

        # Otherwise, we have to preserve the long word intact.  Only add
        # it to the current line if there's nothing already there --
        # that minimizes how much we violate the width constraint.
        elif not cur_line:
            cur_line.append(chunks.pop(0))

        # If we're not allowed to break long words, and there's already
        # text on the current line, do nothing.  Next time through the
        # main loop of _wrap_chunks(), we'll wind up here again, but
        # cur_len will be zero, so the next line will be entirely
        # devoted to the long word that we can't handle right now.

    def _wrap_chunks(self, chunks):
        """_wrap_chunks(chunks : [string]) -> [string]

        Wrap a sequence of text chunks and return a list of lines of
        length 'self.width' or less.  (If 'break_long_words' is false,
        some lines may be longer than this.)  Chunks correspond roughly
        to words and the whitespace between them: each chunk is
        indivisible (modulo 'break_long_words'), but a line break can
        come between any two chunks.  Chunks should not have internal
        whitespace; ie. a chunk is either all whitespace or a "word".
        Whitespace chunks will be removed from the beginning and end of
        lines, but apart from that whitespace is preserved.
        """
        lines = []
        if self.width <= 0:
            raise ValueError("invalid width %r (must be > 0)" % self.width)

        while chunks:

            # Start the list of chunks that will make up the current line.
            # cur_len is just the length of all the chunks in cur_line.
            cur_line = []
            cur_len = 0

            # Figure out which static string will prefix this line.
            if lines:
                indent = self.subsequent_indent
            else:
                indent = self.initial_indent

            # Maximum width for this line.
            width = self.width - len(indent)

            # First chunk on line is whitespace -- drop it, unless this
            # is the very beginning of the text (ie. no lines started yet).
            if string.strip(chunks[0]) == '' and lines:
                del chunks[0]

            while chunks:
                l = len(chunks[0])

                # Can at least squeeze this chunk onto the current line.
                if cur_len + l <= width:
                    cur_line.append(chunks.pop(0))
                    cur_len = cur_len + l

                # Nope, this line is full.
                else:
                    break

            # The current line is full, and the next chunk is too big to
            # fit on *any* line (not just this one).
            if chunks and len(chunks[0]) > width:
                self._handle_long_word(chunks, cur_line, cur_len, width)

            # If the last chunk on this line is all whitespace, drop it.
            if cur_line and string.strip(cur_line[-1]) == '':
                del cur_line[-1]

            # Convert current line back to a string and store it in list
            # of all lines (return value).
            if cur_line:
                lines.append(indent + string.join(cur_line, ''))

        return lines


    # -- Public interface ----------------------------------------------

    def wrap(self, text):
        """wrap(text : string) -> [string]

        Reformat the single paragraph in 'text' so it fits in lines of
        no more than 'self.width' columns, and return a list of wrapped
        lines.  Tabs in 'text' are expanded with string.expandtabs(),
        and all other whitespace characters (including newline) are
        converted to space.
        """
        text = self._munge_whitespace(text)
        indent = self.initial_indent
        chunks = self._split(text)
        if self.fix_sentence_endings:
            self._fix_sentence_endings(chunks)
        return self._wrap_chunks(chunks)

    def fill(self, text):
        """fill(text : string) -> string

        Reformat the single paragraph in 'text' to fit in lines of no
        more than 'self.width' columns, and return a new string
        containing the entire wrapped paragraph.
        """
        return string.join(self.wrap(text), "\n")


# -- Convenience interface ---------------------------------------------

def wrap(text, width=70, **kwargs):
    """Wrap a single paragraph of text, returning a list of wrapped lines.

    Reformat the single paragraph in 'text' so it fits in lines of no
    more than 'width' columns, and return a list of wrapped lines.  By
    default, tabs in 'text' are expanded with string.expandtabs(), and
    all other whitespace characters (including newline) are converted to
    space.  See TextWrapper class for available keyword args to customize
    wrapping behaviour.
    """
    kw = kwargs.copy()
    kw['width'] = width
    w = apply(TextWrapper, (), kw)
    return w.wrap(text)

def fill(text, width=70, **kwargs):
    """Fill a single paragraph of text, returning a new string.

    Reformat the single paragraph in 'text' to fit in lines of no more
    than 'width' columns, and return a new string containing the entire
    wrapped paragraph.  As with wrap(), tabs are expanded and other
    whitespace characters converted to space.  See TextWrapper class for
    available keyword args to customize wrapping behaviour.
    """
    kw = kwargs.copy()
    kw['width'] = width
    w = apply(TextWrapper, (), kw)
    return w.fill(text)


# -- Loosely related functionality -------------------------------------

def dedent(text):
    """dedent(text : string) -> string

    Remove any whitespace than can be uniformly removed from the left
    of every line in `text`.

    This can be used e.g. to make triple-quoted strings line up with
    the left edge of screen/whatever, while still presenting it in the
    source code in indented form.

    For example:

        def test():
            # end first line with \ to avoid the empty line!
            s = '''\
            hello
              world
            '''
            print repr(s)          # prints '    hello\n      world\n    '
            print repr(dedent(s))  # prints 'hello\n  world\n'
    """
    lines = text.expandtabs().split('\n')
    margin = None
    for line in lines:
        content = line.lstrip()
        if not content:
            continue
        indent = len(line) - len(content)
        if margin is None:
            margin = indent
        else:
            margin = min(margin, indent)

    if margin is not None and margin > 0:
        for i in range(len(lines)):
            lines[i] = lines[i][margin:]

    return string.join(lines, '\n')
