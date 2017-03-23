# logutil.py
import logging, ctypes

class NTDebugHandler(logging.Handler):
    def emit(self, record,
             writeA=ctypes.windll.kernel32.OutputDebugStringA,
             writeW=ctypes.windll.kernel32.OutputDebugStringW):
        text = self.format(record)
        if isinstance(text, str):
            writeA(text + "\n")
        else:
            writeW(text + u"\n")
logging.NTDebugHandler = NTDebugHandler

def setup_logging(*pathnames):
    import ConfigParser

    parser = ConfigParser.ConfigParser()
    parser.optionxform = str # use case sensitive option names!

    parser.read(pathnames)

    DEFAULTS = {"handler": "StreamHandler()",
                "format": "%(levelname)s:%(name)s:%(message)s",
                "level": "WARNING"}

    def get(section, option):
        try:
            return parser.get(section, option, True)
        except (ConfigParser.NoOptionError, ConfigParser.NoSectionError):
            return DEFAULTS[option]

    levelname = get("logging", "level")
    format = get("logging", "format")
    handlerclass = get("logging", "handler")

    # convert level name to level value
    level = getattr(logging, levelname)
    # create the handler instance
    handler = eval(handlerclass, vars(logging))
    formatter = logging.Formatter(format)
    handler.setFormatter(formatter)
    logging.root.addHandler(handler)
    logging.root.setLevel(level)

    try:
        for name, value in parser.items("logging.levels", True):
            value = getattr(logging, value)
            logging.getLogger(name).setLevel(value)
    except ConfigParser.NoSectionError:
        pass
