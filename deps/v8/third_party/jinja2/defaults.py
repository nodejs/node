# -*- coding: utf-8 -*-
from ._compat import range_type
from .filters import FILTERS as DEFAULT_FILTERS  # noqa: F401
from .tests import TESTS as DEFAULT_TESTS  # noqa: F401
from .utils import Cycler
from .utils import generate_lorem_ipsum
from .utils import Joiner
from .utils import Namespace

# defaults for the parser / lexer
BLOCK_START_STRING = "{%"
BLOCK_END_STRING = "%}"
VARIABLE_START_STRING = "{{"
VARIABLE_END_STRING = "}}"
COMMENT_START_STRING = "{#"
COMMENT_END_STRING = "#}"
LINE_STATEMENT_PREFIX = None
LINE_COMMENT_PREFIX = None
TRIM_BLOCKS = False
LSTRIP_BLOCKS = False
NEWLINE_SEQUENCE = "\n"
KEEP_TRAILING_NEWLINE = False

# default filters, tests and namespace

DEFAULT_NAMESPACE = {
    "range": range_type,
    "dict": dict,
    "lipsum": generate_lorem_ipsum,
    "cycler": Cycler,
    "joiner": Joiner,
    "namespace": Namespace,
}

# default policies
DEFAULT_POLICIES = {
    "compiler.ascii_str": True,
    "urlize.rel": "noopener",
    "urlize.target": None,
    "truncate.leeway": 5,
    "json.dumps_function": None,
    "json.dumps_kwargs": {"sort_keys": True},
    "ext.i18n.trimmed": False,
}
