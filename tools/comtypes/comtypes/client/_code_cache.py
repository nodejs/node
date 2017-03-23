"""comtypes.client._code_cache helper module.

The main function is _find_gen_dir(), which on-demand creates the
comtypes.gen package and returns a directory where generated code can
be written to.
"""
import ctypes, logging, os, sys, tempfile, types
logger = logging.getLogger(__name__)

def _find_gen_dir():
    """Create, if needed, and return a directory where automatically
    generated modules will be created.

    Usually, this is the directory 'Lib/site-packages/comtypes/gen'.

    If the above directory cannot be created, or if it is not a
    directory in the file system (when comtypes is imported from a
    zip-archive or a zipped egg), or if the current user cannot create
    files in this directory, an additional directory is created and
    appended to comtypes.gen.__path__ .

    For a Python script using comtypes, the additional directory is
    '%APPDATA%\<username>\Python\Python25\comtypes_cache'.

    For an executable frozen with py2exe, the additional directory is
    '%TEMP%\comtypes_cache\<imagebasename>-25'.
    """
    _create_comtypes_gen_package()
    from comtypes import gen
    if not _is_writeable(gen.__path__):
        # check type of executable image to determine a subdirectory
        # where generated modules are placed.
        ftype = getattr(sys, "frozen", None)
        version_str = "%d%d" % sys.version_info[:2]
        if ftype == None:
            # Python script
            subdir = r"Python\Python%s\comtypes_cache" % version_str
            basedir = _get_appdata_dir()

        elif ftype == "dll":
            # dll created with py2exe
            path = _get_module_filename(sys.frozendllhandle)
            base = os.path.splitext(os.path.basename(path))[0]
            subdir = r"comtypes_cache\%s-%s" % (base, version_str)
            basedir = tempfile.gettempdir()

        else: # ftype in ('windows_exe', 'console_exe')
            # exe created by py2exe
            base = os.path.splitext(os.path.basename(sys.executable))[0]
            subdir = r"comtypes_cache\%s-%s" % (base, version_str)
            basedir = tempfile.gettempdir()

        gen_dir = os.path.join(basedir, subdir)
        if not os.path.exists(gen_dir):
            logger.info("Creating writeable comtypes cache directory: '%s'", gen_dir)
            os.makedirs(gen_dir)
        gen.__path__.append(gen_dir)
    result = os.path.abspath(gen.__path__[-1])
    logger.info("Using writeable comtypes cache directory: '%s'", result)
    return result

################################################################

if os.name == "ce":
    SHGetSpecialFolderPath = ctypes.OleDLL("coredll").SHGetSpecialFolderPath
    GetModuleFileName = ctypes.WinDLL("coredll").GetModuleFileNameW
else:
    SHGetSpecialFolderPath = ctypes.OleDLL("shell32.dll").SHGetSpecialFolderPathW
    GetModuleFileName = ctypes.WinDLL("kernel32.dll").GetModuleFileNameW
SHGetSpecialFolderPath.argtypes = [ctypes.c_ulong, ctypes.c_wchar_p,
                                   ctypes.c_int, ctypes.c_int]
GetModuleFileName.restype = ctypes.c_ulong
GetModuleFileName.argtypes = [ctypes.c_ulong, ctypes.c_wchar_p, ctypes.c_ulong]

CSIDL_APPDATA = 26
MAX_PATH = 260

def _create_comtypes_gen_package():
    """Import (creating it if needed) the comtypes.gen package."""
    try:
        import comtypes.gen
        logger.info("Imported existing %s", comtypes.gen)
    except ImportError:
        import comtypes
        logger.info("Could not import comtypes.gen, trying to create it.")
        try:
            comtypes_path = os.path.abspath(os.path.join(comtypes.__path__[0], "gen"))
            if not os.path.isdir(comtypes_path):
                os.mkdir(comtypes_path)
                logger.info("Created comtypes.gen directory: '%s'", comtypes_path)
            comtypes_init = os.path.join(comtypes_path, "__init__.py")
            if not os.path.exists(comtypes_init):
                logger.info("Writing __init__.py file: '%s'", comtypes_init)
                ofi = open(comtypes_init, "w")
                ofi.write("# comtypes.gen package, directory for generated files.\n")
                ofi.close()
        except (OSError, IOError), details:
            logger.info("Creating comtypes.gen package failed: %s", details)
            module = sys.modules["comtypes.gen"] = types.ModuleType("comtypes.gen")
            comtypes.gen = module
            comtypes.gen.__path__ = []
            logger.info("Created a memory-only package.")

def _is_writeable(path):
    """Check if the first part, if any, on path is a directory in
    which we can create files."""
    if not path:
        return False
    try:
        tempfile.TemporaryFile(dir=path[0])
    except (OSError, IOError), details:
        logger.debug("Path is unwriteable: %s", details)
        return False
    return True

def _get_module_filename(hmodule):
    """Call the Windows GetModuleFileName function which determines
    the path from a module handle."""
    path = ctypes.create_unicode_buffer(MAX_PATH)
    if GetModuleFileName(hmodule, path, MAX_PATH):
        return path.value
    raise ctypes.WinError()

def _get_appdata_dir():
    """Return the 'file system directory that serves as a common
    repository for application-specific data' - CSIDL_APPDATA"""
    path = ctypes.create_unicode_buffer(MAX_PATH)
    # get u'C:\\Documents and Settings\\<username>\\Application Data'
    SHGetSpecialFolderPath(0, path, CSIDL_APPDATA, True)
    return path.value
