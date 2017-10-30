#!/bin/sh
#
#       ZLIB compilation script for the OS/400.
#
#
#       This is a shell script since make is not a standard component of OS/400.


################################################################################
#
#                       Tunable configuration parameters.
#
################################################################################

TARGETLIB='ZLIB'                # Target OS/400 program library
STATBNDDIR='ZLIB_A'             # Static binding directory.
DYNBNDDIR='ZLIB'                # Dynamic binding directory.
SRVPGM="ZLIB"                   # Service program.
IFSDIR='/zlib'                  # IFS support base directory.
TGTCCSID='500'                  # Target CCSID of objects
DEBUG='*NONE'                   # Debug level
OPTIMIZE='40'                   # Optimisation level
OUTPUT='*NONE'                  # Compilation output option.
TGTRLS='V6R1M0'                 # Target OS release

export TARGETLIB STATBNDDIR DYNBNDDIR SRVPGM IFSDIR
export TGTCCSID DEBUG OPTIMIZE OUTPUT TGTRLS


################################################################################
#
#                       OS/400 specific definitions.
#
################################################################################

LIBIFSNAME="/QSYS.LIB/${TARGETLIB}.LIB"


################################################################################
#
#                               Procedures.
#
################################################################################

#       action_needed dest [src]
#
#       dest is an object to build
#       if specified, src is an object on which dest depends.
#
#       exit 0 (succeeds) if some action has to be taken, else 1.

action_needed()

{
        [ ! -e "${1}" ] && return 0
        [ "${2}" ] || return 1
        [ "${1}" -ot "${2}" ] && return 0
        return 1
}


#       make_module module_name source_name [additional_definitions]
#
#       Compile source name into module if needed.
#       As side effect, append the module name to variable MODULES.
#       Set LINK to "YES" if the module has been compiled.

make_module()

{
    MODULES="${MODULES} ${1}"
    MODIFSNAME="${LIBIFSNAME}/${1}.MODULE"
    CSRC="`basename \"${2}\"`"

    if action_needed "${MODIFSNAME}" "${2}"
    then    :
    elif [ ! "`sed -e \"/<source name=\\\"${CSRC}\\\">/,/<\\\\/source>/!d\" \
      -e '/<depend /!d'                                                 \
      -e 's/.* name=\"\\([^\"]*\\)\".*/\\1/' < \"${TOPDIR}/treebuild.xml\" |
        while read HDR
        do      if action_needed \"${MODIFSNAME}\" \"${IFSDIR}/include/${HDR}\"
                then    echo recompile
                        break
                fi
        done`" ]
    then    return 0
    fi

    CMD="CRTCMOD MODULE(${TARGETLIB}/${1}) SRCSTMF('${2}')"
    CMD="${CMD} SYSIFCOPT(*IFS64IO) OPTION(*INCDIRFIRST)"
    CMD="${CMD} LOCALETYPE(*LOCALE) FLAG(10)"
    CMD="${CMD} INCDIR('${IFSDIR}/include' ${INCLUDES})"
    CMD="${CMD} TGTCCSID(${TGTCCSID}) TGTRLS(${TGTRLS})"
    CMD="${CMD} OUTPUT(${OUTPUT})"
    CMD="${CMD} OPTIMIZE(${OPTIMIZE})"
    CMD="${CMD} DBGVIEW(${DEBUG})"
    system "${CMD}"
    LINK=YES
}


#       Determine DB2 object name from IFS name.

db2_name()

{
        basename "${1}"                                                 |
        tr 'a-z-' 'A-Z_'                                                |
        sed -e 's/\..*//'                                               \
            -e 's/^\(.\).*\(.........\)$/\1\2/'
}


#       Force enumeration types to be the same size as integers.

copy_hfile()

{
        sed -e '1i\
#pragma enum(int)\
' "${@}" -e '$a\
#pragma enum(pop)\
'
}


################################################################################
#
#                             Script initialization.
#
################################################################################

SCRIPTDIR=`dirname "${0}"`

case "${SCRIPTDIR}" in
/*)     ;;
*)      SCRIPTDIR="`pwd`/${SCRIPTDIR}"
esac

while true
do      case "${SCRIPTDIR}" in
        */.)    SCRIPTDIR="${SCRIPTDIR%/.}";;
        *)      break;;
        esac
done

#  The script directory is supposed to be in ${TOPDIR}/os400.

TOPDIR=`dirname "${SCRIPTDIR}"`
export SCRIPTDIR TOPDIR
cd "${TOPDIR}"


#  Extract the version from the master compilation XML file.

VERSION=`sed -e '/^<package /!d'                                        \
            -e 's/^.* version="\([0-9.]*\)".*$/\1/' -e 'q'              \
            < treebuild.xml`
export VERSION

################################################################################


#       Create the OS/400 library if it does not exist.

if action_needed "${LIBIFSNAME}"
then    CMD="CRTLIB LIB(${TARGETLIB}) TEXT('ZLIB: Data compression API')"
        system "${CMD}"
fi


#       Create the DOCS source file if it does not exist.

if action_needed "${LIBIFSNAME}/DOCS.FILE"
then    CMD="CRTSRCPF FILE(${TARGETLIB}/DOCS) RCDLEN(112)"
        CMD="${CMD} CCSID(${TGTCCSID}) TEXT('Documentation texts')"
        system "${CMD}"
fi

#       Copy some documentation files if needed.

for TEXT in "${TOPDIR}/ChangeLog" "${TOPDIR}/FAQ"                       \
    "${TOPDIR}/README" "${SCRIPTDIR}/README400"
do      MEMBER="${LIBIFSNAME}/DOCS.FILE/`db2_name \"${TEXT}\"`.MBR"

        if action_needed "${MEMBER}" "${TEXT}"
        then    CMD="CPY OBJ('${TEXT}') TOOBJ('${MEMBER}') TOCCSID(${TGTCCSID})"
                CMD="${CMD} DTAFMT(*TEXT) REPLACE(*YES)"
                system "${CMD}"
        fi
done


#       Create the OS/400 source program file for the C header files.

SRCPF="${LIBIFSNAME}/H.FILE"

if action_needed "${SRCPF}"
then    CMD="CRTSRCPF FILE(${TARGETLIB}/H) RCDLEN(112)"
        CMD="${CMD} CCSID(${TGTCCSID}) TEXT('ZLIB: C/C++ header files')"
        system "${CMD}"
fi


#       Create the IFS directory for the C header files.

if action_needed "${IFSDIR}/include"
then    mkdir -p "${IFSDIR}/include"
fi

#       Copy the header files to DB2 library. Link from IFS include directory.

for HFILE in "${TOPDIR}/"*.h
do      DEST="${SRCPF}/`db2_name \"${HFILE}\"`.MBR"

        if action_needed "${DEST}" "${HFILE}"
        then    copy_hfile < "${HFILE}" > tmphdrfile

                #       Need to translate to target CCSID.

                CMD="CPY OBJ('`pwd`/tmphdrfile') TOOBJ('${DEST}')"
                CMD="${CMD} TOCCSID(${TGTCCSID}) DTAFMT(*TEXT) REPLACE(*YES)"
                system "${CMD}"
                # touch -r "${HFILE}" "${DEST}"
                rm -f tmphdrfile
        fi

        IFSFILE="${IFSDIR}/include/`basename \"${HFILE}\"`"

        if action_needed "${IFSFILE}" "${DEST}"
        then    rm -f "${IFSFILE}"
                ln -s "${DEST}" "${IFSFILE}"
        fi
done


#       Install the ILE/RPG header file.


HFILE="${SCRIPTDIR}/zlib.inc"
DEST="${SRCPF}/ZLIB.INC.MBR"

if action_needed "${DEST}" "${HFILE}"
then    CMD="CPY OBJ('${HFILE}') TOOBJ('${DEST}')"
        CMD="${CMD} TOCCSID(${TGTCCSID}) DTAFMT(*TEXT) REPLACE(*YES)"
        system "${CMD}"
        # touch -r "${HFILE}" "${DEST}"
fi

IFSFILE="${IFSDIR}/include/`basename \"${HFILE}\"`"

if action_needed "${IFSFILE}" "${DEST}"
then    rm -f "${IFSFILE}"
        ln -s "${DEST}" "${IFSFILE}"
fi


#      Create and compile the identification source file.

echo '#pragma comment(user, "ZLIB version '"${VERSION}"'")' > os400.c
echo '#pragma comment(user, __DATE__)' >> os400.c
echo '#pragma comment(user, __TIME__)' >> os400.c
echo '#pragma comment(copyright, "Copyright (C) 1995-2017 Jean-Loup Gailly, Mark Adler. OS/400 version by P. Monnerat.")' >> os400.c
make_module     OS400           os400.c
LINK=                           # No need to rebuild service program yet.
MODULES=


#       Get source list.

CSOURCES=`sed -e '/<source name="/!d'                                   \
    -e 's/.* name="\([^"]*\)".*/\1/' < treebuild.xml`

#       Compile the sources into modules.

for SRC in ${CSOURCES}
do      MODULE=`db2_name "${SRC}"`
        make_module "${MODULE}" "${SRC}"
done


#       If needed, (re)create the static binding directory.

if action_needed "${LIBIFSNAME}/${STATBNDDIR}.BNDDIR"
then    LINK=YES
fi

if [ "${LINK}" ]
then    rm -rf "${LIBIFSNAME}/${STATBNDDIR}.BNDDIR"
        CMD="CRTBNDDIR BNDDIR(${TARGETLIB}/${STATBNDDIR})"
        CMD="${CMD} TEXT('ZLIB static binding directory')"
        system "${CMD}"

        for MODULE in ${MODULES}
        do      CMD="ADDBNDDIRE BNDDIR(${TARGETLIB}/${STATBNDDIR})"
                CMD="${CMD} OBJ((${TARGETLIB}/${MODULE} *MODULE))"
                system "${CMD}"
        done
fi


#       The exportation file for service program creation must be in a DB2
#               source file, so make sure it exists.

if action_needed "${LIBIFSNAME}/TOOLS.FILE"
then    CMD="CRTSRCPF FILE(${TARGETLIB}/TOOLS) RCDLEN(112)"
        CMD="${CMD} CCSID(${TGTCCSID}) TEXT('ZLIB: build tools')"
        system "${CMD}"
fi


DEST="${LIBIFSNAME}/TOOLS.FILE/BNDSRC.MBR"

if action_needed "${SCRIPTDIR}/bndsrc" "${DEST}"
then    CMD="CPY OBJ('${SCRIPTDIR}/bndsrc') TOOBJ('${DEST}')"
        CMD="${CMD} TOCCSID(${TGTCCSID}) DTAFMT(*TEXT) REPLACE(*YES)"
        system "${CMD}"
        # touch -r "${SCRIPTDIR}/bndsrc" "${DEST}"
        LINK=YES
fi


#       Build the service program if needed.

if action_needed "${LIBIFSNAME}/${SRVPGM}.SRVPGM"
then    LINK=YES
fi

if [ "${LINK}" ]
then    CMD="CRTSRVPGM SRVPGM(${TARGETLIB}/${SRVPGM})"
        CMD="${CMD} SRCFILE(${TARGETLIB}/TOOLS) SRCMBR(BNDSRC)"
        CMD="${CMD} MODULE(${TARGETLIB}/OS400)"
        CMD="${CMD} BNDDIR(${TARGETLIB}/${STATBNDDIR})"
        CMD="${CMD} TEXT('ZLIB ${VERSION} dynamic library')"
        CMD="${CMD} TGTRLS(${TGTRLS})"
        system "${CMD}"
        LINK=YES

        #       Duplicate the service program for a versioned backup.

        BACKUP=`echo "${SRVPGM}${VERSION}"                              |
                sed -e 's/.*\(..........\)$/\1/' -e 's/\./_/g'`
        BACKUP="`db2_name \"${BACKUP}\"`"
        BKUPIFSNAME="${LIBIFSNAME}/${BACKUP}.SRVPGM"
        rm -f "${BKUPIFSNAME}"
        CMD="CRTDUPOBJ OBJ(${SRVPGM}) FROMLIB(${TARGETLIB})"
        CMD="${CMD} OBJTYPE(*SRVPGM) NEWOBJ(${BACKUP})"
        system "${CMD}"
fi


#       If needed, (re)create the dynamic binding directory.

if action_needed "${LIBIFSNAME}/${DYNBNDDIR}.BNDDIR"
then    LINK=YES
fi

if [ "${LINK}" ]
then    rm -rf "${LIBIFSNAME}/${DYNBNDDIR}.BNDDIR"
        CMD="CRTBNDDIR BNDDIR(${TARGETLIB}/${DYNBNDDIR})"
        CMD="${CMD} TEXT('ZLIB dynamic binding directory')"
        system "${CMD}"
        CMD="ADDBNDDIRE BNDDIR(${TARGETLIB}/${DYNBNDDIR})"
        CMD="${CMD} OBJ((*LIBL/${SRVPGM} *SRVPGM))"
        system "${CMD}"
fi
