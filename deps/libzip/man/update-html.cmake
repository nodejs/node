# expect variables IN and OUT
EXECUTE_PROCESS(COMMAND mandoc -T html -Oman=%N.html,style=../nih-man.css ${IN}
  OUTPUT_VARIABLE HTML)
SET(LINKBASE "http://pubs.opengroup.org/onlinepubs/9699919799/functions/")
STRING(REGEX REPLACE "(<a class=\"Xr\" href=\")([^\"]*)(\">)" "\\1${LINKBASE}\\2\\3" HTML "${HTML}")
STRING(REGEX REPLACE "${LINKBASE}(libzip|zip)" "\\1" HTML "${HTML}")
STRING(REGEX REPLACE "NetBSD [0-9.]*" "NiH" HTML "${HTML}")
FILE(WRITE ${OUT}.new "${HTML}")
CONFIGURE_FILE(${OUT}.new ${OUT} COPYONLY)
FILE(REMOVE ${OUT}.new)


