#! /bin/sh -efu

# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").
# You may not use this file except in compliance with the License.
# You can obtain a copy in the file LICENSE in the source distribution
# or at https://www.openssl.org/source/license.html

# The script takes starts with PATCH_BRANCH (or commit derived from TAG_PRE_FMT
# and openssl version derived from OPENSSL_BRANCH, if none provided), applies
# the list of patches provided in the command line, rebases the resulting
# branch to TAG_PRE_FMT-derived-tagged commit, then iterates over each
# of the branch commits and processes the files with extensions specified
# in FMT_EXTENSIONS (except for the ones in EXCLUDE_FILES) with CLANG_FMT_CMD,
# and committing the result on top of TAG_POST_FMT-derived-tagged commit.
# The result of successful processing is saved to OUT_DIR with
# git format-patch, and, if PATCH_BRANCH is a local branch name, it is reset
# to the resulting branch.

: "${TAG_PRE_FMT=%s-PRE-CLANG-FORMAT-WEBKIT}"
: "${TAG_POST_FMT=%s-POST-CLANG-FORMAT-WEBKIT}"
: "${GIT_CMD=git}"
: "${GIT_REMOTE=origin}"
: "${CLANG_FMT_CMD=clang-format-21}"
: "${EXCLUDE_FILES=crypto/asn1/charmap.h crypto/bn/bn_prime.h crypto/conf/conf_def.h crypto/objects/obj_dat.h crypto/objects/obj_xref.h include/openssl/obj_mac.h}"
: "${FMT_EXTENSIONS=.c .h .c.in .h.in}"
: "${WORK_BRANCH_PRE=reformat-patches-pre}"
: "${WORK_BRANCH_POST=reformat-patches-post}"
: "${PROCESS_BRANCH_PRE=reformat-patches-process-pre}"

: "${GIT_REPO_URL=https://github.com/openssl/openssl.git}"
: "${NO_CLEANUP=0}"
: "${GIT_REPO_DIR=}"
: "${OUT_DIR=out}"
: "${NO_FORMAT_PATCH=0}"
: "${OPENSSL_BRANCH=master}"
: "${PATCH_BRANCH=}"
: "${FORCE=0}"
: "${DO_REBASE_AFTER=0}"
: "${NO_RESET_ON_SUCCESS=0}"

cleanup_done=1
branches_created=0

prn()
{
    printf >&2 "%s\n" "$@"
}

msg()
{
    printf >&2 "$0: %s\n" "$*"
}

die()
{
    msg "$*"
    exit 1
}

exit_handler()
{
    [ 0 = "${cleanup_done}" ] || return;

    if [ -n "${WORKTREE_DIR-}" ]; then
        msg "The temporarily created worktree is located" \
            "at '${WORKTREE_DIR}', feel free to remove it (after" \
            "it is no longer needed) with" \
            "$GIT_CMD ${GIT_REPO_DIR:+-C ${GIT_REPO_DIR} }worktree remove" \
            "-f '${WORKTREE_DIR}' && rm -rf '${WORKTREE_DIR}'"
    fi

    if [ 0 = "${PERMANENT_GIT_DIR:-}" -a -n "${GIT_REPO_DIR:-}" ]; then
        msg "The temporarily created git repository directory is located" \
            "at '${GIT_REPO_DIR}', feel free to remove it after" \
            "it is no longer needed."
    fi

    [ 0 = "${branches_created}" ] ||
        msg "The temporarily created working branches ('${WORK_BRANCH_PRE}'," \
            "'${WORK_BRANCH_POST}', and '${PROCESS_BRANCH_PRE}')" \
            "are not removed."

    cleanup_done=1
}

trap exit_handler 0 TERM INT QUIT

# Check that the working branches are available for us to use
check_branch()
{
    branch_name=$(eval "printf '%s' \"\${$1}\"")

    if "$GIT_CMD" -C "${GIT_REPO_DIR}" show-ref --verify --quiet \
             "refs/heads/${branch_name}"; then
        die "'${branch_name}' branch exists already in '${GIT_REPO_DIR}';" \
            "please specify -f option or a different working branch name" \
            "in $1 environment variable"
    fi

    return 0
}

usage()
{
    prn "Usage: $0 [-g GIT_REPO_DIR] [-D] [-u GIT_REPO_URL] [-o OUT_DIR] [-O]" \
        "       [-b OPENSSL_BRANCH] [-B PATCH_BRANCH] [-f] [-R] [-n] [-h]" \
        "       [patch...]"
}

help()
{
    prn "" \
        "Re-format OpenSSL patches using clang-format." \
        "" \
        "A script applies patches on top of a pre-reformat-tagged commit," \
        "processes them with clang-format, and re-generates them on top" \
        "of the corresponding post-reformat-tagged commit." \
        "" \
        "OPTIONS:" \
        "    -g     Path to a local openssl repository;  if no local" \
        "           directory is specified, the repository is checked out" \
        "           from GIT_REPO_URL into a temporary directory" \
        "           (Current: '${GIT_REPO_DIR}')." \
        "    -D     Do not remove the temporarily created repository" \
        "           (Current: '${NO_CLEANUP}')." \
        "    -u     URL for cloning the openssl repository, if no git" \
        "           repository directory was provided" \
        "           (Current: '${GIT_REPO_URL}')." \
        "    -o     Output directory for patches (Current: '${OUT_DIR}')." \
        "    -O     Do not output the resulting patches with git format-patch" \
        "           (Current: '${NO_FORMAT_PATCH}')." \
        "    -b     openssl branch to work on, should be 'master'" \
        "           or 'openssl-X.Y' (Current: '${OPENSSL_BRANCH}')." \
        "    -B     If non-empty, the provided revision is used as a base" \
        "           commit to work on: the provided patches are applied" \
        "           on top of it (if any);  if a local branch name" \
        "           is provided, it will be reset to the resulting patch set" \
        "           upon success, unless -n option is specified" \
        "           (Current: '${PATCH_BRANCH}')." \
        "    -f     Allow overwriting working branches" \
        "           (WORK_BRANCH_PRE='${WORK_BRANCH_PRE}'," \
        "           WORK_BRANCH_POST='${WORK_BRANCH_POST}'," \
        "           PROCESS_BRANCH_PRE='${PROCESS_BRANCH_PRE}') if they exist" \
        "           already (Current: '${FORCE}')." \
        "    -R     Try to rebase the branch on top of OPENSSL_BRANCH" \
        "           after the processing (Current: '${DO_REBASE_AFTER}')." \
        "    -n     Do not reset PATCH_BRANCH to the result of processing" \
        "           on success (Current: '${NO_RESET_ON_SUCCESS}')." \
        "    -h     Show this help message and exit." \
        "    patch  Path to a patch file(s) to process, required" \
        "           if no PATCH_BRANCH is specified.  If PATCH_BRANCH" \
        "           is provided, patches are applied on top of it, otherwise" \
        "           applied on top of pre-reformat-tagged commit, that" \
        "           is referenced by tag name constructed from TAG_PRE_FMT" \
        "           and version derived from the openssl branch provided" \
        "           in -b option/OPENSSL_BRANCH." \
        "" \
        "ENVIRONMENT:" \
        "    TAG_PRE_FMT" \
        "        Format of the pre-format tag, it is passed as a format" \
        "        string to printf with openssl version (either '4.0'" \
        "        for the master branch or the remainder after removal" \
        "        of 'openssl-' prefix in the OPENSSL_BRANCH value)" \
        "        as the only argument to yield the name of the git tag" \
        "        that is considered the last commit before the reformatting" \
        "        with clang-format took place (Current: '${TAG_PRE_FMT}')." \
        "    TAG_POST_FMT" \
        "        Format of the post-format tag, semantics is similar" \
        "        to TAG_PRE_FMT, but with respect to the first commit" \
        "        after the clang-format reformatting" \
        "        (Current: '${TAG_POST_FMT}')." \
        "    GIT_CMD" \
        "        git command (Current: '${GIT_CMD}')." \
        "    GIT_REMOTE" \
        "        Remote to track (Current: '${GIT_REMOTE}')." \
        "    CLANG_FMT_CMD" \
        "        clang-format command (Current: '${CLANG_FMT_CMD}')." \
        "    EXCLUDE_FILES" \
        "        Space-separated list of files to exclude from clang-format" \
        "        processing, as they are generated with make update" \
        "        (Current: '${EXCLUDE_FILES}')." \
        "    FMT_EXTENSIONS" \
        "        List of extensions of files to process with clang-format" \
        "        (Current: '${FMT_EXTENSIONS}')." \
        "    WORK_BRANCH_PRE " \
        "        Name of a temporary branch for pre-reformatted commits" \
        "        (Current: '${WORK_BRANCH_PRE}')." \
        "    WORK_BRANCH_POST " \
        "        Name of a temporary branch for post-reformatted commits" \
        "        (Current: '${WORK_BRANCH_POST}')." \
        "    PROCESS_BRANCH_PRE " \
        "        Name of a temporary branch for tracking reformatting" \
        "        progress (it walks from TAG_PRE to WORK_BRANCH_PRE" \
        "        during the course of processing)" \
        "        (Current: '${PROCESS_BRANCH_PRE}')." \
        "    GIT_REPO_URL" \
        "        URL to openssl git repository, can be overridden" \
        "        with -u option." \
        "    GIT_REPO_DIR" \
        "        openssl git repository dir, can be overridden" \
        "        with -g option." \
        "    NO_CLEANUP" \
        "        If not set to 0, skip removal of work branches, worktree," \
        "        and a temporarily created git repository after processing," \
        "        can be overridden with -D option." \
        "    OUT_DIR" \
        "        Output directory for patches, can be overridden" \
        "        with -o option." \
        "    NO_FORMAT_PATCH" \
        "        If set to 1, skip calling git format-patch on the resulting" \
        "        branch in order to store the results on the OUT_DIR," \
        "        can be overridden with -O option." \
        "    OPENSSL_BRANCH" \
        "        openssl branch to work on, can be overridden with -b option." \
        "    PATCH_BRANCH" \
        "        If non-empty, uses the branch as the base commit" \
        "        for processing, can be overridden with -B option." \
        "    FORCE" \
        "        If not set to 1, script aborts if any of WORK_BRANCH_PRE," \
        "        WORK_BRANCH_POST, or PROCESS_BRANCH_PRE branches exists" \
        "        before the start of processing." \
        "    DO_REBASE_AFTER" \
        "        If set to 1, try to perform rebase on top of OPENSSL_BRANCH" \
        "        after processing, can be overridden with -R option." \
        "    NO_RESET_ON_SUCCESS" \
        "        If not set to 0, do not reset PATH_BRANCH after a successful" \
        "        processing, can be overridden with -n option." \
        "" \
        "EXAMPLES:" \
        "    Updating a patch set against a stable branch that can be applied" \
        "    on top of pre-reformat-tagged commit:" \
        "" \
        "        $0 -b openssl-3.5 -o out_dir my_patches/*.patch" \
        "" \
        "    It will create a temporary repository, perform the processing" \
        "    there, and output the patches into the specified directory." \
        "" \
        "" \
        "    Updating a branch in an existing repository and rebase" \
        "    it on top of the current master:" \
        "" \
        "        $0 -g openssl_repo -B my_branch -O -R" \
        "" \
        "    It will process the patches, rebase them on top of the default" \
        "    branch (master), and then reset the provided branch name" \
        "    upon success."
}

while getopts ":g:Du:o:Ob:B:fRnh" opt; do
    case "${opt}" in
    g) GIT_REPO_DIR="${OPTARG}"   ;;
    D) NO_CLEANUP=1               ;;
    u) GIT_REPO_URL="${OPTARG}"   ;;
    o) OUT_DIR="${OPTARG}"        ;;
    O) NO_FORMAT_PATCH=1          ;;
    b) OPENSSL_BRANCH="${OPTARG}" ;;
    B) PATCH_BRANCH="${OPTARG}"   ;;
    f) FORCE=1                    ;;
    R) DO_REBASE_AFTER=1          ;;
    n) NO_RESET_ON_SUCCESS=1      ;;
    h)
        usage
        help
        exit 0
        ;;
    ?)
        msg "Unknown option '-${OPTARG}', see $0 -h for more information."
        usage
        exit 1
        ;;
    esac
done

shift "$((OPTIND - 1))"

[ 0 -eq "$#" -o "x--" != "x${1-}" ] || shift

# Check that we have work to do
if [ -z "${PATCH_BRANCH}" -a 1 -gt "$#" ]; then
    usage
    die "PATCH_BRANCH is empty and no patches supplied on the command line, exiting"
fi

if [ 1 != "${FORCE}" -a -n "${GIT_REPO_DIR}" ]; then
    check_branch 'WORK_BRANCH_PRE'
    check_branch 'WORK_BRANCH_POST'
    check_branch 'PROCESS_BRANCH_PRE'
fi

# Command-line checks are done
cleanup_done=0

# Getting the repo
PERMANENT_GIT_DIR=1
if [ -z "${GIT_REPO_DIR}" ]; then
    PERMANENT_GIT_DIR=0
    GIT_REPO_DIR=$(mktemp -d "$(pwd)/reformat-openssl-XXXXXX")
    "$GIT_CMD" clone "${GIT_REPO_URL}" "${GIT_REPO_DIR}"
fi
if [ 0 != "${NO_CLEANUP}" ]; then
    msg "Created a temporary directory for the repo: ${GIT_REPO_DIR}"
fi

# Determine the tag name
if [ "master" = "${OPENSSL_BRANCH}" ]; then
    TAG_PREFIX=4.0
else
    # Check that we can extract the tag prefix first
    [ "x${OPENSSL_BRANCH#openssl-}" != "x${OPENSSL_BRANCH}" ] ||
        die "Can't parse branch name: '${OPENSSL_BRANCH}'," \
            "only 'master' and 'openssl-X.Y' are supported."
    TAG_PREFIX="${OPENSSL_BRANCH#openssl-}"
fi
TAG_PRE=$(printf "${TAG_PRE_FMT}" "${TAG_PREFIX}")
TAG_POST=$(printf "${TAG_POST_FMT}" "${TAG_PREFIX}")

# Checking that PATCH_BRANCH doesn't include TAG_POST already
if [ -n "${PATCH_BRANCH}" ]; then
    if "$GIT_CMD" -C "${GIT_REPO_DIR}" merge-base --is-ancestor "${TAG_POST}" "$PATCH_BRANCH"; then
        die "PATCH_BRANCH ('${PATCH_BRANCH}') already includes" \
            "post-reformat-tagged ('${TAG_POST}') commit, exiting."
    fi
fi


# Create the worktree
WORKTREE_DIR=$(mktemp -d "$(pwd)/reformat-openssl-worktree-XXXXXX")
"$GIT_CMD" -C "$GIT_REPO_DIR" worktree add "${WORKTREE_DIR}" "${TAG_PRE}"

# Get the branches set up
BASE_COMMIT="${PATCH_BRANCH}"
[ -n "$BASE_COMMIT" ] || BASE_COMMIT="${TAG_PRE}"
"$GIT_CMD" -C "$WORKTREE_DIR" branch -f "${WORK_BRANCH_POST}" "${TAG_POST}"
"$GIT_CMD" -C "$WORKTREE_DIR" branch -f "${WORK_BRANCH_PRE}" "${BASE_COMMIT}"
"$GIT_CMD" -C "$WORKTREE_DIR" branch -u "${GIT_REMOTE}/${OPENSSL_BRANCH}" "${WORK_BRANCH_PRE}"
branches_created=1

# Apply the patches
while [ 0 -lt "$#" ]; do
    patch_path=$(realpath "$1")
    "$GIT_CMD" -C "${WORKTREE_DIR}" am "${patch_path}"
    shift
done

# Working inside the worktree from now on
(
cd "${WORKTREE_DIR}"

# Rebase the branch
"$GIT_CMD" checkout "${WORK_BRANCH_PRE}"
"$GIT_CMD" rebase "${TAG_PRE}"

# Iterate over the commits and process each with clang-format
"$GIT_CMD" log --reverse --pretty="%H" "${TAG_PRE}..${WORK_BRANCH_PRE}" \
    | while read -r commit; do
        "$GIT_CMD" branch -f "${PROCESS_BRANCH_PRE}" "$commit"
        "$GIT_CMD" checkout "${PROCESS_BRANCH_PRE}"
        msg "Processing $("$GIT_CMD" log --pretty=oneline HEAD^..HEAD)"
        # Process only the touched files
        "$GIT_CMD" show --pretty="" --name-status --no-renames "$commit" \
            | while read -r line; do
                # Skip deletions
                [ "x${line}" = "x${line#D}" ] || continue

                fname="${line#*	}"

                do_process=0
                # Process only *.c *.h *.c.in *.h.in
                for i in ${FMT_EXTENSIONS}; do
                    if [ "x${fname}" != "x${fname%${i}}" ]; then
                        do_process=1
                        break
                    fi
                done

                # Process the exclusion list
                for i in ${EXCLUDE_FILES}; do
                    if [ "x${fname}" = "x${i}" ]; then
                        do_process=0
                        break;
                    fi
                done

                if [ 1 = "${do_process}" ]; then
                    msg "  Formatting ${fname}"
                    "$CLANG_FMT_CMD" -i --style=file:.clang-format "$fname"
                else
                    msg "  Including ${fname} without processing"
                fi
                "$GIT_CMD" add "$fname"
            done

        "$GIT_CMD" commit --amend --no-edit
        "$GIT_CMD" checkout "${WORK_BRANCH_POST}"

        "$GIT_CMD" show --pretty="" --name-status --no-renames "${PROCESS_BRANCH_PRE}" \
            | while read -r line; do
                fname="${line#*	}"

                # Process deletions
                if [ "x${line}" != "x${line#D}" ]; then
                    "$GIT_CMD" rm "$fname"
                    continue
                fi

                "$GIT_CMD" reset "${PROCESS_BRANCH_PRE}" -- "$fname"
            done

        "$GIT_CMD" commit -C "${commit}"
        "$GIT_CMD" reset --hard
    done

# Rebase WORK_BRANCH_POST on top of OPENSSL_BRANCH
if [ 1 = "${DO_REBASE_AFTER}" ]; then
    "$GIT_CMD" checkout "${WORK_BRANCH_POST}"
    "$GIT_CMD" rebase "${OPENSSL_BRANCH}"
fi

# Reset PATCH_BRANCH to WORK_BRANCH_POST if the former is a ref
if [ 0 = "${NO_RESET_ON_SUCCESS}" ]; then
    if "$GIT_CMD" show-ref --verify --quiet "refs/heads/${PATCH_BRANCH}"; then
        msg "Resetting branch '${PATCH_BRANCH}'" \
            "from $("$GIT_CMD" show-ref "refs/heads/${PATCH_BRANCH}")" \
            "to $("$GIT_CMD" show-ref refs/heads/"${WORK_BRANCH_POST}")"
        "$GIT_CMD" branch -f "${PATCH_BRANCH}" "${WORK_BRANCH_POST}"
    fi
fi
) # End of the subshell with pwd in the worktree

# Output the patches
if [ 1 != "${NO_FORMAT_PATCH}" ]; then
    mkdir -p "${OUT_DIR}"
    OUT_DIR=$(realpath "${OUT_DIR}")
    "$GIT_CMD" -C "${WORKTREE_DIR}" format-patch -o "${OUT_DIR}" \
        "${TAG_POST}..${WORK_BRANCH_POST}"
    msg "The resulting patches are saved at '${OUT_DIR}'"
else
    if [ 1 = "${PERMANENT_GIT_DIR}" -a 0 != "${NO_RESET_ON_SUCCESS}" ]; then
        msg "The resulting patches are in the '${WORK_BRANCH_POST}' branch"
    fi
fi

# Cleanup
if [ 0 = "${NO_CLEANUP}" ]; then
    if [ -n "${WORKTREE_DIR-}" ]; then
        "$GIT_CMD" -C "${WORKTREE_DIR}" worktree remove -f "${WORKTREE_DIR}"
        rm -rf "${WORKTREE_DIR}" || :
    fi

    if [ 1 = "${PERMANENT_GIT_DIR}" ]; then
        # Removing the working branches
        "$GIT_CMD" -C "${GIT_REPO_DIR}" branch -D "${WORK_BRANCH_PRE}" || :
        "$GIT_CMD" -C "${GIT_REPO_DIR}" branch -D "${PROCESS_BRANCH_PRE}" || :

        # Leaving WORK_BRANCH_POST if the branch has not been reset
        # and the patches haven't been output
        if [ 1 != "${NO_FORMAT_PATCH}" -o 0 = "${NO_RESET_ON_SUCCESS}" ]; then
            "$GIT_CMD" -C "${GIT_REPO_DIR}" branch -D "${WORK_BRANCH_POST}" || :
        fi
    else
        # Removing the temporarily created git repo
        rm -rf "${GIT_REPO_DIR}" || :
    fi
fi

cleanup_done=1

exit 0
