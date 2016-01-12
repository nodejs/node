//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// rlregr.c
//
// assembly regression worker for rl.c


#include "rl.h"


#define TMP_PREFIX "di"   // 2 characters

// Currently not overrideable
#define REGR_PERL "perl"

#define DIR_START_CMD "startdir.cmd"
#define DIR_END_CMD "enddir.cmd"

// <rl> ... </rl> directives:
//
// PassFo
//      Doesn't add -FoNUL. to the command line; allows passing of -Fo
//      explicitly.
//
// NoAsm
//      Disables asm file generation.

#define PASS_FO_CMD "PassFo"
#define NO_ASM_CMD "NoAsm"

//
// Global variables set before worker threads start, and only accessed
// (not set) by the worker threads.
//

static BOOL NoMasterCompare;


LOCAL int
DoCommand(
    char *path,     // full path of directory to run command in
    char *cmdbuf,
    bool displayError = true
)
{
    int rtncode;

    rtncode = ExecuteCommand(path, cmdbuf);
    if (rtncode && displayError) {
        LogOut("ERROR: Command had a return code of %d:", rtncode);
        LogOut("    %s", cmdbuf);
    }
    return rtncode;
}

LOCAL void __cdecl
RegrCleanUp(void)
{
}

// One-time assembly regression initialization.
void
RegrInit(
    void
)
{
    atexit(RegrCleanUp);

    if (getenv_unsafe("REGR_NOMASTERCMP") != NULL) {
        if (FBaseline) {
            Fatal("-baseline conflicts with environment variable REGR_NOMASTERCMP.");
        }
        NoMasterCompare = TRUE;
    }
    else {
        NoMasterCompare = FALSE;
    }
}

// Called at the start of each directory.
BOOL
RegrStartDir(
    char *path
)
{
    char full[MAX_PATH];

    // Execute directory setup command script if present.

    sprintf_s(full, "%s\\%s", path, DIR_START_CMD);
    if (GetFileAttributes(full) != INVALID_FILE_ATTRIBUTES) {
        Message(""); // newline
        Message("Executing %s", DIR_START_CMD);
        if (DoCommand(path, DIR_START_CMD))
            return FALSE;
        Message(""); // newline
    }

    return TRUE;
}

// Called at the end of each directory.
BOOL
RegrEndDir(
    char* path
)
{
    char full[MAX_PATH];

    // Execute directory shutdown command script if present.

    sprintf_s(full, "%s\\%s", path, DIR_END_CMD);
    if (GetFileAttributes(full) != INVALID_FILE_ATTRIBUTES) {
        Message(""); // newline
        Message("Executing %s", DIR_END_CMD);
        if (DoCommand(path, DIR_END_CMD))
            return FALSE;
        Message("");
    }

    return TRUE;
}

// Called to perform assembly regressions on one file.
// Returns -1 for failure, 1 for diff, 0 for okay.
int
RegrFile(
    CDirectory* pDir,
    Test * pTest,
    TestVariant * pTestVariant
)
{
    FILE *fp;
    char *p, *opts;
    int x;
    int rc;
    int retval;
    char full[MAX_PATH];              // temporary place for full paths
    char basename[MAX_PATH];          // base filename
    char *asmdir;                     // dir of generated asm file
    char masterasmbuf[MAX_PATH];      // name of master asm file
    char asmbuf[MAX_PATH];            // name of generated asm file
    char diffbuf[MAX_PATH];           // name of generated diff file
    char objbuf[MAX_PATH];            // name of generated obj file
    char fullmasterasmbuf[MAX_PATH];  // path of master asm file
    char fullasmbuf[MAX_PATH];        // path of generated asm file
    char fulldiffbuf[MAX_PATH];       // path of generated diff file
    char fullobjbuf[MAX_PATH];        // path of generated obj file
    char standardoptbuf[BUFFER_SIZE]; // standard compilation options (-Fa..)
    char cmdbuf[BUFFER_SIZE];         // compilation command
    char compilerbuf[BUFFER_SIZE];    // compiler name buffer
    char buf[BUFFER_SIZE];
    char tmp_file1[BUFFER_SIZE];
    char tmp_file2[BUFFER_SIZE];
    char tmp_file3[BUFFER_SIZE];
    BOOL passFo = FALSE;
    BOOL noAsm = FALSE;

    // We don't allow non-target conditions for ASM currently, so we should
    // never see a conditionNodeList.

    ASSERTNR(pTest->conditionNodeList == NULL);

    // Check the directives.

    if (HasInfo(pTest->defaultTestInfo.data[TIK_RL_DIRECTIVES], XML_DELIM,
       PASS_FO_CMD))
    {
       passFo = TRUE;
    }
    if (HasInfo(pTest->defaultTestInfo.data[TIK_RL_DIRECTIVES], XML_DELIM,
       NO_ASM_CMD))
    {
       noAsm = TRUE;
    }

    // Base file name (name.* -> name)

    strcpy_s(basename, pTest->name);
    for(p = basename; *p != '.' && *p != '\0'; p++);
    *p = '\0';


    // Determine the standard options.

    asmdir = pDir->IsBaseline() ? MASTER_DIR : DIFF_DIR;
    sprintf_s(standardoptbuf, "-AsmDumpMode:%s\\%s.asm",
        asmdir, basename);

    switch (TargetMachine) {
    case TM_WVM:
    case TM_WVMX86:
    case TM_WVM64:
        strcat_s(standardoptbuf, " /BC ");
        break;
    }

    if (BaseCompiler || DiffCompiler) {
        char *b2;

        if (pDir->IsBaseline())
            b2 = BaseCompiler;
        else
            b2 = DiffCompiler;

        ASSERTNR(b2);

        sprintf_s(compilerbuf, " -B2 %s", b2);
    }
    else
    {
        *compilerbuf = '\0';
    }

    // Try to open the source file.

    sprintf_s(full, "%s\\%s", pDir->GetDirectoryPath(), pTest->name);
    fp = fopen_unsafe(full, "r");
    if (fp == NULL) {
        LogError("ERROR: Could not open '%s' with error '%s'", pTest->name, strerror_unsafe(errno));
        return -1;
    }
    fclose(fp);

    ASSERTNR(pTestVariant->optFlags == NULL);
    ASSERTNR(pTestVariant->testInfo.data[TIK_LINK_FLAGS] == NULL);

    opts = pTestVariant->testInfo.data[TIK_COMPILE_FLAGS];
    if (opts == NULL)
    {
       opts = "";
    }

    // Compute the object name if passing -Fo through.

    if (passFo)
        sprintf_s(objbuf, "%s.obj", basename);
    else
        sprintf_s(objbuf, "%s\\%s.obj", asmdir, basename);
    sprintf_s(fullobjbuf, "%s\\%s", pDir->GetDirectoryPath(), objbuf);

    // Compute the names for the assembly and diff files.

    sprintf_s(masterasmbuf, "%s\\%s.asm", MASTER_DIR, basename);
    sprintf_s(asmbuf,       "%s\\%s.asm", asmdir, basename);
    sprintf_s(diffbuf,      "%s\\%s.d",   DIFF_DIR, basename);

    sprintf_s(fullmasterasmbuf, "%s\\%s", pDir->GetDirectoryPath(), masterasmbuf);
    sprintf_s(fullasmbuf,       "%s\\%s", pDir->GetDirectoryPath(), asmbuf);
    sprintf_s(fulldiffbuf,      "%s\\%s", pDir->GetDirectoryPath(), diffbuf);

    // There could be a diff file from previous regressions.  Remove it now.

    if (!pDir->IsBaseline())
        DeleteFileIfFound(fulldiffbuf);

    // Create the compiler command and do the compile.

    sprintf_s(cmdbuf, "%s%s %s %s %s %s 2>nul 1>nul",
        JCBinary, compilerbuf, standardoptbuf, opts, EXTRA_CC_FLAGS,
       pTest->name);

    if (FVerbose) {
        strcpy_s(buf, cmdbuf);
    }
    else {
        // For non-verbose, don't display the standard options
        sprintf_s(buf, "%s%s %s %s %s",
            JCBinary, compilerbuf, opts, EXTRA_CC_FLAGS, pTest->name);
    }
    Message("%s", buf);

    if (FTest)
    {
       return 0;
    }

    // There could be a diff file from previous regressions.  Remove it now.

    if (!pDir->IsBaseline())
    {
        DeleteFileIfFound(fulldiffbuf);
    }

    if (DoCommand(pDir->GetDirectoryPath(), cmdbuf))
    {
        DeleteFileIfFound(fullasmbuf);
        return -1;
    }

    // If we are passing -Fo through to the compiler, remove the generated
    // object file.

    if (passFo)
        DeleteFileRetryMsg(fullobjbuf);

    // See if we generated an assembly file.

    if (GetFileAttributes(fullasmbuf) == INVALID_FILE_ATTRIBUTES) {
        LogOut("ERROR: Assembly file %s not generated", asmbuf);
        return -1;
    }

    // Create the assembler command and do the asm if necessary.

    if (REGR_ASM && !noAsm) {
        sprintf_s(cmdbuf, "%s %s", REGR_ASM, asmbuf);
        Message("%s", cmdbuf);
        // ignore the REGR_ASM return code; we still want to do diffs or
        // update the master with the generated asm file.
        if (DoCommand(pDir->GetDirectoryPath(), cmdbuf)) {
            retval = -1;
        }
        else {
            retval = 0;
            DeleteFileRetryMsg(fullobjbuf);
        }
    } else {
        retval = 0;
    }

    // Exit if not comparing to master or doing baselines.

    if (NoMasterCompare || pDir->IsBaseline())
        return retval;

    // FYI: Up to this point we haven't used fullmasterasmbuf.

    ASSERTNR(!strcmp(asmdir, DIFF_DIR));

    // If the master doesn't exist, consider this a failure.

    if (GetFileAttributes(fullmasterasmbuf) == INVALID_FILE_ATTRIBUTES) {
        LogOut("ERROR: %s doesn't exist", fullmasterasmbuf);
        return -1;
    }

    x = DoCompare(fullasmbuf, fullmasterasmbuf);
    if (x < 0)
        return -1;

    // If the files differ, remove potential bogus differences and try again.

    rc = retval;
    if (x) {

        // Remove the possible bogus diffs.

        // Generate some temporary file names.
        // Note: these are full pathnames, not relative pathnames.
        // NOTE: BE VERY CAREFUL TO CLEAN THESE UP!

        if (mytmpnam(pDir->GetDirectoryPath(), TMP_PREFIX, tmp_file1) == NULL ||
            mytmpnam(pDir->GetDirectoryPath(), TMP_PREFIX, tmp_file2) == NULL ||
            mytmpnam(pDir->GetDirectoryPath(), TMP_PREFIX, tmp_file3) == NULL) {
            Fatal("Unable to create temporary files");
        }

        ThreadInfo[ThreadId].AddToTmpFileList(tmp_file1);
        ThreadInfo[ThreadId].AddToTmpFileList(tmp_file2);
        ThreadInfo[ThreadId].AddToTmpFileList(tmp_file3);

        sprintf_s(cmdbuf, "%s %s %s %s",
            REGR_SHOWD, REGR_PERL, fullasmbuf, tmp_file1);
        if (DoCommand(pDir->GetDirectoryPath(), cmdbuf)) {
            rc = -1;
        }
        else {
            sprintf_s(cmdbuf, "%s %s %s %s",
                REGR_SHOWD, REGR_PERL, fullmasterasmbuf, tmp_file2);
            if (DoCommand(pDir->GetDirectoryPath(), cmdbuf)) {
                rc = -1;
            }
            else {

                // Compare again.

                x = DoCompare(tmp_file1, tmp_file2);
                if (x < 0) {
                    rc = -1;
                }
                else if (x) {

                    // They still differ, make a diff file.

                    sprintf_s(cmdbuf, "%s %s %s > %s",
                        REGR_DIFF, tmp_file1, tmp_file2, tmp_file3);
                    DoCommand(pDir->GetDirectoryPath(), cmdbuf, false);
                    sprintf_s(cmdbuf,"%s -p -e \"s/^[<>-].*\\r?\\n//\" < %s > %s",
                        REGR_PERL, tmp_file3, fulldiffbuf);
                    if (DoCommand(pDir->GetDirectoryPath(), cmdbuf)) {
                        rc = -1;
                    }
                    else {
                        Message("%s and %s differ", asmbuf, masterasmbuf);
                        WriteLog("%s has diffs", pTest->name);
                        rc = 1;
                    }
                }
                else {
                    // Move the new asm into the master directory

                    DeleteFileRetryMsg(fullmasterasmbuf);
                    if (!MoveFile(fullasmbuf, fullmasterasmbuf)) {
                        LogOut("ERROR: MoveFile(%s, %s) failed with error %d",
                            fullasmbuf, fullmasterasmbuf, GetLastError());
                        rc = -1;
                    }
                }
            }
        }

        ThreadInfo[ThreadId].DeleteTmpFileList();
    }
    else {

        // They don't differ, so delete the assembly file we just created.

        DeleteFileRetryMsg(fullasmbuf);
    }

    return rc;
}
