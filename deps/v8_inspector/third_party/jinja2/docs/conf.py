# -*- coding: utf-8 -*-
#
# Jinja2 documentation build configuration file, created by
# sphinx-quickstart on Sun Apr 27 21:42:41 2008.
#
# This file is execfile()d with the current directory set to its containing dir.
#
# The contents of this file are pickled, so don't put values in the namespace
# that aren't pickleable (module imports are okay, they're removed automatically).
#
# All configuration values have a default value; values that are commented out
# serve to show the default value.

import sys, os

# If your extensions are in another directory, add it here. If the directory
# is relative to the documentation root, use os.path.abspath to make it
# absolute, like shown here.
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

# General configuration
# ---------------------

# Add any Sphinx extension module names here, as strings. They can be extensions
# coming with Sphinx (named 'sphinx.ext.*') or your custom ones.
extensions = ['sphinx.ext.autodoc', 'jinjaext']

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# The suffix of source filenames.
source_suffix = '.rst'

# The master toctree document.
master_doc = 'index'

# General substitutions.
project = 'Jinja2'
copyright = '2008, Armin Ronacher'

# The default replacements for |version| and |release|, also used in various
# other places throughout the built documents.
#
# The short X.Y version.
import pkg_resources
try:
    release = pkg_resources.get_distribution('Jinja2').version
except ImportError:
    print 'To build the documentation, The distribution information of Jinja2'
    print 'Has to be available.  Either install the package into your'
    print 'development environment or run "setup.py develop" to setup the'
    print 'metadata.  A virtualenv is recommended!'
    sys.exit(1)
if 'dev' in release:
    release = release.split('dev')[0] + 'dev'
version = '.'.join(release.split('.')[:2])

# There are two options for replacing |today|: either, you set today to some
# non-false value, then it is used:
#today = ''
# Else, today_fmt is used as the format for a strftime call.
today_fmt = '%B %d, %Y'

# List of documents that shouldn't be included in the build.
#unused_docs = []

# If true, '()' will be appended to :func: etc. cross-reference text.
#add_function_parentheses = True

# If true, the current module name will be prepended to all description
# unit titles (such as .. function::).
#add_module_names = True

# If true, sectionauthor and moduleauthor directives will be shown in the
# output. They are ignored by default.
#show_authors = False

# The name of the Pygments (syntax highlighting) style to use.
pygments_style = 'jinjaext.JinjaStyle'


# Options for HTML output
# -----------------------

html_theme = 'jinja'
html_theme_path = ['_themes']

# The name for this set of Sphinx documents.  If None, it defaults to
# "<project> v<release> documentation".
#html_title = None

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']

# If not '', a 'Last updated on:' timestamp is inserted at every page bottom,
# using the given strftime format.
html_last_updated_fmt = '%b %d, %Y'

# If true, SmartyPants will be used to convert quotes and dashes to
# typographically correct entities.
#html_use_smartypants = True

# no modindex
html_use_modindex = False

# If true, the reST sources are included in the HTML build as _sources/<name>.
#html_copy_source = True

# If true, an OpenSearch description file will be output, and all pages will
# contain a <link> tag referring to it.
#html_use_opensearch = False

# Output file base name for HTML help builder.
htmlhelp_basename = 'Jinja2doc'


# Options for LaTeX output
# ------------------------

# The paper size ('letter' or 'a4').
latex_paper_size = 'a4'

# The font size ('10pt', '11pt' or '12pt').
#latex_font_size = '10pt'

# Grouping the document tree into LaTeX files. List of tuples
# (source start file, target name, title, author, document class [howto/manual]).
latex_documents = [
  ('latexindex', 'Jinja2.tex', 'Jinja2 Documentation', 'Armin Ronacher',
   'manual'),
]

# Additional stuff for LaTeX
latex_elements = {
    'fontpkg':      r'\usepackage{mathpazo}',
    'papersize':    'a4paper',
    'pointsize':    '12pt',
    'preamble':     r'''
\usepackage{jinjastyle}

% i hate you latex
\DeclareUnicodeCharacter{14D}{o}
'''
}

latex_use_parts = True

latex_additional_files = ['jinjastyle.sty', 'logo.pdf']

# If false, no module index is generated.
latex_use_modindex = False

html_sidebars = {
    'index':    ['sidebarlogo.html', 'sidebarintro.html', 'sourcelink.html',
                 'searchbox.html'],
    '**':       ['sidebarlogo.html', 'localtoc.html', 'relations.html',
                 'sourcelink.html', 'searchbox.html']
}
