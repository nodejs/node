import types
import os
import sys
import comtypes.client
import comtypes.tools.codegenerator

import logging
logger = logging.getLogger(__name__)

__verbose__ = __debug__

if os.name == "ce":
    # Windows CE has a hard coded PATH
    # XXX Additionally there's an OEM path, plus registry settings.
    # We don't currently use the latter.
    PATH = ["\\Windows", "\\"]
else:
    PATH = os.environ["PATH"].split(os.pathsep)

def _my_import(fullname):
    # helper function to import dotted modules
    import comtypes.gen
    if comtypes.client.gen_dir \
           and comtypes.client.gen_dir not in comtypes.gen.__path__:
        comtypes.gen.__path__.append(comtypes.client.gen_dir)
    return __import__(fullname, globals(), locals(), ['DUMMY'])

def _name_module(tlib):
    # Determine the name of a typelib wrapper module.
    libattr = tlib.GetLibAttr()
    modname = "_%s_%s_%s_%s" % \
              (str(libattr.guid)[1:-1].replace("-", "_"),
               libattr.lcid,
               libattr.wMajorVerNum,
               libattr.wMinorVerNum)
    return "comtypes.gen." + modname

def GetModule(tlib):
    """Create a module wrapping a COM typelibrary on demand.

    'tlib' must be an ITypeLib COM pointer instance, the pathname of a
    type library, or a tuple/list specifying the arguments to a
    comtypes.typeinfo.LoadRegTypeLib call:

      (libid, wMajorVerNum, wMinorVerNum, lcid=0)

    Or it can be an object with _reg_libid_ and _reg_version_
    attributes.

    A relative pathname is interpreted as relative to the callers
    __file__, if this exists.

    This function determines the module name from the typelib
    attributes, then tries to import it.  If that fails because the
    module doesn't exist, the module is generated into the
    comtypes.gen package.

    It is possible to delete the whole comtypes\gen directory to
    remove all generated modules, the directory and the __init__.py
    file in it will be recreated when needed.

    If comtypes.gen __path__ is not a directory (in a frozen
    executable it lives in a zip archive), generated modules are only
    created in memory without writing them to the file system.

    Example:

        GetModule("shdocvw.dll")

    would create modules named

       comtypes.gen._EAB22AC0_30C1_11CF_A7EB_0000C05BAE0B_0_1_1
       comtypes.gen.SHDocVw

    containing the Python wrapper code for the type library used by
    Internet Explorer.  The former module contains all the code, the
    latter is a short stub loading the former.
    """
    pathname = None
    if isinstance(tlib, basestring):
        # pathname of type library
        if not os.path.isabs(tlib):
            # If a relative pathname is used, we try to interpret
            # this pathname as relative to the callers __file__.
            frame = sys._getframe(1)
            _file_ = frame.f_globals.get("__file__", None)
            if _file_ is not None:
                directory = os.path.dirname(os.path.abspath(_file_))
                abspath = os.path.normpath(os.path.join(directory, tlib))
                # If the file does exist, we use it.  Otherwise it may
                # still be that the file is on Windows search path for
                # typelibs, and we leave the pathname alone.
                if os.path.isfile(abspath):
                    tlib = abspath
        logger.debug("GetModule(%s)", tlib)
        pathname = tlib
        tlib = comtypes.typeinfo.LoadTypeLibEx(tlib)
    elif isinstance(tlib, (tuple, list)):
        # sequence containing libid and version numbers
        logger.debug("GetModule(%s)", (tlib,))
        tlib = comtypes.typeinfo.LoadRegTypeLib(comtypes.GUID(tlib[0]), *tlib[1:])
    elif hasattr(tlib, "_reg_libid_"):
        # a COMObject implementation
        logger.debug("GetModule(%s)", tlib)
        tlib = comtypes.typeinfo.LoadRegTypeLib(comtypes.GUID(tlib._reg_libid_),
                                                *tlib._reg_version_)
    else:
        # an ITypeLib pointer
        logger.debug("GetModule(%s)", tlib.GetLibAttr())

    # create and import the module
    mod = _CreateWrapper(tlib, pathname)
    try:
        modulename = tlib.GetDocumentation(-1)[0]
    except comtypes.COMError:
        return mod
    if modulename is None:
        return mod
    if sys.version_info < (3, 0):
        modulename = modulename.encode("mbcs")

    # create and import the friendly-named module
    try:
        mod = _my_import("comtypes.gen." + modulename)
    except Exception, details:
        logger.info("Could not import comtypes.gen.%s: %s", modulename, details)
    else:
        return mod
    # the module is always regenerated if the import fails
    if __verbose__:
        print "# Generating comtypes.gen.%s" % modulename
    # determine the Python module name
    fullname = _name_module(tlib)
    modname = fullname.split(".")[-1]
    code = "from comtypes.gen import %s\nglobals().update(%s.__dict__)\n" % (modname, modname)
    code += "__name__ = 'comtypes.gen.%s'" % modulename
    if comtypes.client.gen_dir is None:
        mod = types.ModuleType("comtypes.gen." + modulename)
        mod.__file__ = os.path.join(os.path.abspath(comtypes.gen.__path__[0]),
                                    "<memory>")
        exec code in mod.__dict__
        sys.modules["comtypes.gen." + modulename] = mod
        setattr(comtypes.gen, modulename, mod)
        return mod
    # create in file system, and import it
    ofi = open(os.path.join(comtypes.client.gen_dir, modulename + ".py"), "w")
    ofi.write(code)
    ofi.close()
    return _my_import("comtypes.gen." + modulename)

def _CreateWrapper(tlib, pathname=None):
    # helper which creates and imports the real typelib wrapper module.
    fullname = _name_module(tlib)
    try:
        return sys.modules[fullname]
    except KeyError:
        pass

    modname = fullname.split(".")[-1]

    try:
        return _my_import(fullname)
    except Exception, details:
        logger.info("Could not import %s: %s", fullname, details)

    # generate the module since it doesn't exist or is out of date
    from comtypes.tools.tlbparser import generate_module
    if comtypes.client.gen_dir is None:
        import cStringIO
        ofi = cStringIO.StringIO()
    else:
        ofi = open(os.path.join(comtypes.client.gen_dir, modname + ".py"), "w")
    # XXX use logging!
    if __verbose__:
        print "# Generating comtypes.gen.%s" % modname
    generate_module(tlib, ofi, pathname)

    if comtypes.client.gen_dir is None:
        code = ofi.getvalue()
        mod = types.ModuleType(fullname)
        mod.__file__ = os.path.join(os.path.abspath(comtypes.gen.__path__[0]),
                                    "<memory>")
        exec code in mod.__dict__
        sys.modules[fullname] = mod
        setattr(comtypes.gen, modname, mod)
    else:
        ofi.close()
        mod = _my_import(fullname)
    return mod

################################################################

if __name__ == "__main__":
    # When started as script, generate typelib wrapper from .tlb file.
    GetModule(sys.argv[1])
