Flask Sphinx Styles
===================

This repository contains sphinx styles for Flask and Flask related
projects.  To use this style in your Sphinx documentation, follow
this guide:

1. put this folder as _themes into your docs folder.  Alternatively
   you can also use git submodules to check out the contents there.
2. add this to your conf.py:

   sys.path.append(os.path.abspath('_themes'))
   html_theme_path = ['_themes']
   html_theme = 'flask'

The following themes exist:

- 'flask' - the standard flask documentation theme for large
  projects
- 'flask_small' - small one-page theme.  Intended to be used by
  very small addon libraries for flask.

The following options exist for the flask_small theme:

   [options]
   index_logo = ''              filename of a picture in _static
                                to be used as replacement for the
                                h1 in the index.rst file.
   index_logo_height = 120px    height of the index logo
   github_fork = ''             repository name on github for the
                                "fork me" badge
