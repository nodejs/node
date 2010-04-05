#!/usr/bin/env python
# encoding: utf-8
# Ali Sabil, 2007

"""
To use this module do not forget to call
opt.tool_options('gnu_dirs')
AND
conf.check_tool('gnu_dirs')

Add options for the standard GNU directories, this tool will add the options
found in autotools, and will update the environment with the following
installation variables:

 * PREFIX : architecture-independent files [/usr/local]
 * EXEC_PREFIX : architecture-dependent files [PREFIX]
 * BINDIR : user executables [EXEC_PREFIX/bin]
 * SBINDIR : user executables [EXEC_PREFIX/sbin]
 * LIBEXECDIR : program executables [EXEC_PREFIX/libexec]
 * SYSCONFDIR : read-only single-machine data [PREFIX/etc]
 * SHAREDSTATEDIR : modifiable architecture-independent data [PREFIX/com]
 * LOCALSTATEDIR : modifiable single-machine data [PREFIX/var]
 * LIBDIR : object code libraries [EXEC_PREFIX/lib]
 * INCLUDEDIR : C header files [PREFIX/include]
 * OLDINCLUDEDIR : C header files for non-gcc [/usr/include]
 * DATAROOTDIR : read-only arch.-independent data root [PREFIX/share]
 * DATADIR : read-only architecture-independent data [DATAROOTDIR]
 * INFODIR : info documentation [DATAROOTDIR/info]
 * LOCALEDIR : locale-dependent data [DATAROOTDIR/locale]
 * MANDIR : man documentation [DATAROOTDIR/man]
 * DOCDIR : documentation root [DATAROOTDIR/doc/telepathy-glib]
 * HTMLDIR : html documentation [DOCDIR]
 * DVIDIR : dvi documentation [DOCDIR]
 * PDFDIR : pdf documentation [DOCDIR]
 * PSDIR : ps documentation [DOCDIR]
"""

import Utils, Options

_options = [x.split(', ') for x in '''
bindir, user executables, ${EXEC_PREFIX}/bin
sbindir, system admin executables, ${EXEC_PREFIX}/sbin
libexecdir, program executables, ${EXEC_PREFIX}/libexec
sysconfdir, read-only single-machine data, ${PREFIX}/etc
sharedstatedir, modifiable architecture-independent data, ${PREFIX}/com
localstatedir, modifiable single-machine data, ${PREFIX}/var
libdir, object code libraries, ${EXEC_PREFIX}/lib
includedir, C header files, ${PREFIX}/include
oldincludedir, C header files for non-gcc, /usr/include
datarootdir, read-only arch.-independent data root, ${PREFIX}/share
datadir, read-only architecture-independent data, ${DATAROOTDIR}
infodir, info documentation, ${DATAROOTDIR}/info
localedir, locale-dependent data, ${DATAROOTDIR}/locale
mandir, man documentation, ${DATAROOTDIR}/man
docdir, documentation root, ${DATAROOTDIR}/doc/${PACKAGE}
htmldir, html documentation, ${DOCDIR}
dvidir, dvi documentation, ${DOCDIR}
pdfdir, pdf documentation, ${DOCDIR}
psdir, ps documentation, ${DOCDIR}
'''.split('\n') if x]

def detect(conf):
	def get_param(varname, default):
		return getattr(Options.options, varname, '') or default

	env = conf.env
	env['EXEC_PREFIX'] = get_param('EXEC_PREFIX', env['PREFIX'])
	env['PACKAGE'] = Utils.g_module.APPNAME

	complete = False
	iter = 0
	while not complete and iter < len(_options) + 1:
		iter += 1
		complete = True
		for name, help, default in _options:
			name = name.upper()
			if not env[name]:
				try:
					env[name] = Utils.subst_vars(get_param(name, default), env)
				except TypeError:
					complete = False
	if not complete:
		lst = [name for name, _, _ in _options if not env[name.upper()]]
		raise Utils.WafError('Variable substitution failure %r' % lst)

def set_options(opt):

	inst_dir = opt.add_option_group('Installation directories',
'By default, "waf install" will put the files in\
 "/usr/local/bin", "/usr/local/lib" etc. An installation prefix other\
 than "/usr/local" can be given using "--prefix", for example "--prefix=$HOME"')

	for k in ('--prefix', '--destdir'):
		option = opt.parser.get_option(k)
		if option:
			opt.parser.remove_option(k)
			inst_dir.add_option(option)

	inst_dir.add_option('--exec-prefix',
		help = 'installation prefix [Default: ${PREFIX}]',
		default = '',
		dest = 'EXEC_PREFIX')

	dirs_options = opt.add_option_group('Pre-defined installation directories', '')

	for name, help, default in _options:
		option_name = '--' + name
		str_default = default
		str_help = '%s [Default: %s]' % (help, str_default)
		dirs_options.add_option(option_name, help=str_help, default='', dest=name.upper())

