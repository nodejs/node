# AS_CASE(WORD, [PATTERN1], [IF-MATCHED1]...[DEFAULT])
# ----------------------------------------------------
# Expand into
# | case WORD in
# | PATTERN1) IF-MATCHED1 ;;
# | ...
# | *) DEFAULT ;;
# | esac
m4_define([_AS_CASE],
[m4_if([$#], 0, [m4_fatal([$0: too few arguments: $#])],
       [$#], 1, [  *) $1 ;;],
       [$#], 2, [  $1) m4_default([$2], [:]) ;;],
       [  $1) m4_default([$2], [:]) ;;
$0(m4_shiftn(2, $@))])dnl
])
m4_defun([AS_CASE],
[m4_ifval([$2$3],
[case $1 in
_AS_CASE(m4_shift($@))
esac])])

