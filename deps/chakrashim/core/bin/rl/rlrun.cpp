//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// rlrun.c
//
// executable regression worker for rl.c


#include "rl.h"

#define TMP_PREFIX "ex"   // 2 characters


#define POGO_PGD "rlpogo.pgd"

// In the RL environment running Pogo tests, some warnings in the optimization
// compile should be errors, since we do the instrumentation and optimization
// compiles back-to-back.
//   4951 "'%s' has been edited since profile data was collected, function profile data not used"
//   4952 "'%s' : no profile data found in program database '%s'"
//   4953 "Inlinee '%s' has been edited since profile data was collected, profile data not used"
//   4961 "No profile data was merged into '%s', profile-guided optimizations disabled"
//   4962 "Profile-guided optimizations disabled because profile data became inconsistent"
//   4963 "'%s' : no profile data found; different compiler options were used in instrumented build"

static char *PogoForceErrors = "-we4951 -we4952 -we4953 -we4961 -we4962 -we4963";

//
// Global variables set before worker threads start, and only accessed
// (not set) by the worker threads.
//
// sets of options to iterate over
char *OptFlags[MAXOPTIONS + 1], *PogoOptFlags[MAXOPTIONS + 1];

// use a big global array as scratch pad for passing the child process env vars
#define MAX_ENV_LEN 10000
__declspec(thread) char EnvFlags[MAX_ENV_LEN];

//
// Global variables read and written by the worker threads: these need to
// either be protected by synchronization or use thread-local storage.
//

// currently, none


LOCAL void __cdecl
    RunCleanUp()
{
}

void
    RunInit()
{
    char *opts;    // value of EXEC_TESTS_FLAGS environment variable
    int numOptions;
    int numPogoOptions;
    int i;

    atexit(RunCleanUp);

    // Break EXEC_TESTS up into different sets of flags.  The sets should
    // be separated by semi-colons.  Options don't apply to Pogo testing
    // unless prefixed with POGO_TEST_PREFIX.  Those options _only_ apply
    // to Pogo tests.

    opts = EXEC_TESTS_FLAGS;
    ASSERTNR(opts);

    numOptions = numPogoOptions = 0;

    while (opts) {
        while (isspace(*opts))
            opts++;
        if (*opts == '\0')
            break;

        if (!_strnicmp(opts, POGO_TEST_PREFIX, strlen(POGO_TEST_PREFIX))) {
            opts += strlen(POGO_TEST_PREFIX);
            PogoOptFlags[numPogoOptions] = opts;
            ++numPogoOptions;
            if (numPogoOptions == MAXOPTIONS)
                Fatal("Too many options in EXEC_TESTS_FLAGS");
        }
        else {
            OptFlags[numOptions] = opts;
            ++numOptions;
            if (numOptions == MAXOPTIONS)
                Fatal("Too many options in EXEC_TESTS_FLAGS");
        }

        opts = strchr(opts, ';');
        if (opts)
            *opts++ = '\0';
    }

    for (i = 0; i < numPogoOptions; i++) {
        if (strstr(PogoOptFlags[i], "GL") == NULL) {
            Fatal("Pogo without LTCG is not supported");
        }
    }

    OptFlags[numOptions] = NULL;
    PogoOptFlags[numPogoOptions] = NULL;

    if (FVerbose) {
        printf("(Normal) Exec flags:");
        for (i = 0; i < numOptions; i++) {
            printf(" '%s'", OptFlags[i]);
        }
        printf("\nPogo Exec flags:");
        for (i = 0; i < numPogoOptions; i++) {
            printf(" '%s'", PogoOptFlags[i]);
        }
        printf("\n");
    }
}

BOOL
    RunStartDir(
    char * /*dir -- unused*/
    )
{
    return TRUE;
}

void
    DumpFileToLog(
    char* path
    )
{
    FILE* fp;
    char buf[BUFFER_SIZE];
    char* p;

    fp = fopen_unsafe(path, "r");
    if (fp == NULL) {
        LogError("ERROR: DumpFileToLog couldn't open file '%s' with error '%s'", path, strerror_unsafe(errno));
    }
    else {
        int fd = _fileno(fp);
        struct _stat64 fileStats;
        if(fd != -1 && _fstat64(fd, &fileStats) != -1)
        {
            char creationTime[256];
            char accessTime[256];
            char currTime[256];
            __time64_t now = _time64(NULL);
            _ctime64_s(currTime, &now);
            _ctime64_s(creationTime, &fileStats.st_ctime);
            _ctime64_s(accessTime, &fileStats.st_atime);
            auto stripNewline = [](char *buf) {
                if(char *ptr = strchr(buf, '\n'))
                    *ptr = '\0';
            };
            stripNewline(creationTime);
            stripNewline(accessTime);
            stripNewline(currTime);

            LogOut("ERROR: name of output file: %s; size: %I64d; creation: %s, last access: %s, now: %s", path, fileStats.st_size, creationTime, accessTime, currTime);
        }
        LogOut("ERROR: bad output file follows ============");
        while (fgets(buf, BUFFER_SIZE, fp) != NULL) {
            // Strip the newline, since LogOut adds one
            p = strchr(buf, '\n');
            if (p != NULL) {
                *p = '\0';
            }
            LogOut("%s", buf);
        }
        fclose(fp);
        LogOut("ERROR: end of bad output file  ============");
    }
}

// Use a state machine to recognize the word "pass"
BOOL
    LookForPass(
    char *p
    )
{
    int state = 0;

    for(; *p != '\0'; p++) {
        switch(tolower(*p)) {
        case 'p':
            state = 1;
            break;
        case 'a':
            if (state == 1)
                state = 2;
            else
                state = 0;
            break;
        case 's':
            if (state == 2)
                state = 3;
            else if (state == 3)
                return TRUE;
            else
                state = 0;
            break;
        default:
            state = 0;
        }
    }
    return FALSE;
}

// Return TRUE if the specified test is Pogo-specific.
BOOL
    IsPogoTest(
    Test * pTest
    )
{
    return HasInfo(pTest->defaultTestInfo.data[TIK_TAGS], XML_DELIM, "Pogo");
}

// Return TRUE if the specified test should NOT use nogpfnt.obj.
BOOL
    SuppressNoGPF(
    Test * pTest
    )
{
    return HasInfo(pTest->defaultTestInfo.data[TIK_RL_DIRECTIVES], XML_DELIM,
        "NoGPF");
}

template <size_t bufSize>
void
    FillNoGPFFlags(
    char (&nogpfFlags)[bufSize],
    BOOL fSuppressNoGPF
    )
{
    nogpfFlags[0] = '\0';
    if (FNogpfnt && TargetInfo[TargetMachine].fUseNoGPF) {
        if (!fSuppressNoGPF) {
            sprintf_s(nogpfFlags,
                " %s\\bin\\%s\\nogpfnt.obj /entry:nogpfntStartup",
                REGRESS, TargetInfo[TargetMachine].name);
        }
    }
}

BOOL
    CheckForPass(char * filename, char * optReportBuf, char * cmdbuf, BOOL fDumpOutputFile = TRUE)
{
    FILE * fp;
    char buf[BUFFER_SIZE];

    // Check to see if the exe ran at all.

    fp = fopen_unsafe(filename, "r");
    if (fp == NULL) {
        LogOut("ERROR: Test failed to run. Unable to open file '%s', error '%s' (%s):", filename, strerror_unsafe(errno), optReportBuf);
        LogOut("    %s", cmdbuf);
        return FALSE;
    }

    // Parse the output file and verify that all lines must be pass/passed, or empty lines
    BOOL pass = FALSE;
    while(fgets(buf, BUFFER_SIZE, fp) != NULL)
    {
        if(!_strcmpi(buf, "pass\n") || !_strcmpi(buf, "passed\n"))
        {
            // Passing strings were found - pass
            pass = TRUE;
        }
        else if(_strcmpi(buf, "\n") != 0)
        {
            // Something else other than a newline was found - this is a failure.
            pass = FALSE;
            break;
        }
    }

    fclose(fp);

    if (!pass)
    {
        LogOut("ERROR: Test failed to run correctly: pass not found in output file (%s):", optReportBuf);
        LogOut("    %s", cmdbuf);
        if (fDumpOutputFile)
        {
            DumpFileToLog(filename);
        }
    }

    return pass;
}

void CopyRebaseFile(PCSTR testout, PCSTR baseline)
{
    if (FRebase)
    {
        char rebase[_MAX_PATH];
        sprintf_s(rebase, "%s.rebase", baseline);
        CopyFile(testout, rebase, FALSE);
    }
}

// Handle external test scripts.  We support three kinds, makefiles (rl.mak),
// command shell (dotest.cmd), and JScript (*.js).
//
// Standardized makefiles have the following targets:
//   clean: delete all generated files
//   build: build the test (OPT=compile options)
//   run: run the test
//   copy: copy the generated files to a subdirectory (COPYDIR=subdir)
//
int
    DoOneExternalTest(
    CDirectory* pDir,
    TestVariant *pTestVariant,
    char *optFlags,
    char *inCCFlags,
    char *inLinkFlags,
    char *testCmd,
    ExternalTestKind kind,
    BOOL fSyncVariationWhenFinished,
    BOOL fCleanBefore,
    BOOL fCleanAfter,
    BOOL fSuppressNoGPF,
    void *envFlags,
    DWORD millisecTimeout
    )
{
#define NMAKE "nmake -nologo -R -f "
    char full[MAX_PATH];
    char cmdbuf[BUFFER_SIZE];
    char buf[BUFFER_SIZE];
    char ccFlags[BUFFER_SIZE];
    char linkFlags[BUFFER_SIZE];
    char nogpfFlags[BUFFER_SIZE];
    char optReportBuf[BUFFER_SIZE];
    char nonZeroReturnBuf[BUFFER_SIZE];
    char *reason = NULL;
    time_t start_variation;
    UINT elapsed_variation;
    time_t start_build_variation;
    UINT elapsed_build_variation;
    LARGE_INTEGER start_run, end_run, frequency;
    UINT elapsed_run;
    BOOL fFailed = FALSE;
    BOOL fDumpOutputFile = FVerbose;
    BOOL fFileToDelete = FALSE;
    int  cmdResult;
    static unsigned int testCount = 0;
    unsigned int localTestCount = InterlockedIncrement(&testCount);

    // Avoid conditionals by copying/creating ccFlags appropriately.

    if (inCCFlags)
        sprintf_s(ccFlags, " %s", inCCFlags);
    else
        ccFlags[0] = '\0';

    switch (TargetMachine) {
    case TM_WVM:
    case TM_WVMX86:
    case TM_WVM64:
        strcat_s(ccFlags, " /BC ");
        break;
    }

    if (inLinkFlags)
        strcpy_s(linkFlags, inLinkFlags);
    else
        linkFlags[0] = '\0';

    sprintf_s(optReportBuf, "%s%s%s", optFlags, *linkFlags ? ";" : "", linkFlags);

    // Update the status.

    sprintf_s(buf, " (%s)", optReportBuf);
    ThreadInfo[ThreadId].SetCurrentTest(pDir->GetDirectoryName(),
        buf, pDir->IsBaseline());
    UpdateTitleStatus();

    // Make sure the file that will say pass or fail is not present.

    sprintf_s(full, "%s\\testout%d", pDir->GetDirectoryPath(), localTestCount);
    DeleteFileIfFound(full);

    start_variation = time(NULL);

    if (kind == TK_MAKEFILE) {
        Message(""); // newline
        Message("Processing %s with '%s' flags",
            testCmd, optReportBuf);
        Message(""); // newline

        if (FTest)
        {
            return 0;
        }

        if (fCleanBefore) {

            // Clean the directory.

            sprintf_s(cmdbuf, NMAKE"%s clean", testCmd);
            Message(cmdbuf);
            ExecuteCommand(pDir->GetDirectoryPath(), cmdbuf);
        }

        FillNoGPFFlags(nogpfFlags, fSuppressNoGPF);

        // Build the test.

        start_build_variation = time(NULL);

        sprintf_s(cmdbuf, NMAKE"%s build OPT=\"%s %s%s\" LINKFLAGS=\"%s %s %s\"",
            testCmd,
            optFlags, EXTRA_CC_FLAGS, ccFlags,
            LINKFLAGS, linkFlags, nogpfFlags);
        if (strlen(cmdbuf) > BUFFER_SIZE - 1)
            Fatal("Buffer overrun");

        Message(cmdbuf);
        fFailed = ExecuteCommand(pDir->GetDirectoryPath(), cmdbuf);

        elapsed_build_variation = (int)(time(NULL) - start_build_variation);

        if (Timing & TIME_VARIATION) {
            Message("RL: Variation elapsed time (build) (%s, %s, %s): %02d:%02d",
                pDir->GetDirectoryName(),
                "rl.mak",
                optReportBuf,
                elapsed_build_variation / 60, elapsed_build_variation % 60);
        }

        if (fFailed) {
            reason = "build failure";
            goto logFailure;
        }

        // Run the test.

        QueryPerformanceCounter(&start_run);

        sprintf_s(cmdbuf, NMAKE"%s run", testCmd);
        Message(cmdbuf);
        cmdResult = ExecuteCommand(pDir->GetDirectoryPath(), cmdbuf, millisecTimeout);

        QueryPerformanceCounter(&end_run);
        QueryPerformanceFrequency(&frequency);
        elapsed_run = (int) (((end_run.QuadPart - start_run.QuadPart) * 1000UI64) / frequency.QuadPart);

        if (Timing & TIME_VARIATION) {
            Message("RL: Variation elapsed time (run) (%s, %s, %s): %02d:%02d.%03d",
                pDir->GetDirectoryName(),
                "rl.mak",
                optReportBuf,
                elapsed_run / 60000, (elapsed_run % 60000)/1000, elapsed_run % 1000);
        }
    }
    else if (kind == TK_CMDSCRIPT)
    {

        // Build up the test command string

        sprintf_s(cmdbuf, "%s %s %s%s >testout%d", testCmd, optFlags, EXTRA_CC_FLAGS, ccFlags, localTestCount);

        Message("Running '%s'", cmdbuf);

        if (FTest)
        {
            return 0;
        }
        cmdResult = ExecuteCommand(pDir->GetDirectoryPath(), cmdbuf, millisecTimeout, envFlags);
    }
    else if (kind == TK_JSCRIPT || kind==TK_HTML || kind == TK_COMMAND)
    {
        char tempExtraCCFlags[MAX_PATH] = {0};

        // Only append when EXTRA_CC_FLAGS isn't empty.
        if (EXTRA_CC_FLAGS[0])
        {
            // Append test case unique identifier to the end of EXTRA_CC_FLAGS.
            if (FAppendTestNameToExtraCCFlags)
            {
                sprintf_s(tempExtraCCFlags, "%s.%s", EXTRA_CC_FLAGS, pTestVariant->testInfo.data[TIK_FILES]);
            }
            else
            {
                strcpy_s(tempExtraCCFlags, EXTRA_CC_FLAGS);
            }
        }

        char* cmd = JCBinary;
        if (kind != TK_JSCRIPT && kind != TK_HTML)
        {
            cmd = pTestVariant->testInfo.data[TIK_COMMAND];
        }
        sprintf_s(cmdbuf, "%s %s %s %s %s >testout%d 2>&1", cmd, optFlags, tempExtraCCFlags, ccFlags, testCmd, localTestCount);

        Message("Running '%s'", cmdbuf);

        if(FTest)
        {
            DeleteFileIfFound(full);
            return 0;
        }

        cmdResult = ExecuteCommand(pDir->GetDirectoryPath(), cmdbuf, millisecTimeout, envFlags);

        if (cmdResult && cmdResult != WAIT_TIMEOUT && !pTestVariant->testInfo.data[TIK_BASELINE]) // failure code, not baseline diffing
        {
            fFailed = TRUE;
            sprintf_s(nonZeroReturnBuf, "non-zero (%08X) return value from test command", cmdResult);
            reason = nonZeroReturnBuf;
            goto logFailure;
        }
    }
    else
    {
        ASSERTNR(UNREACHED);
        cmdResult = NOERROR; // calm compiler warning about uninitialized variable usage
    }

    // Check for timeout.

    if (cmdResult == WAIT_TIMEOUT) {
        ASSERT(millisecTimeout != INFINITE);
        sprintf_s(nonZeroReturnBuf, "timed out after %u second%s", millisecTimeout / 1000, millisecTimeout == 1000 ? "" : "s");
        reason = nonZeroReturnBuf;
        fFailed = TRUE;
        goto logFailure;
    }

    // If we have a baseline test, we need to check the baseline file.
    if (pTestVariant->testInfo.data[TIK_BASELINE]) {
        char baseline_file[_MAX_PATH];

        sprintf_s(baseline_file, "%s\\%s", pDir->GetDirectoryPath(),
            pTestVariant->testInfo.data[TIK_BASELINE]);
        if (DoCompare(baseline_file, full)) {
            reason = "diffs from baseline";
            sprintf_s(optReportBuf, "%s", baseline_file);
            fFailed = TRUE;
            CopyRebaseFile(full, baseline_file);
        }
    }
    else if ((kind == TK_JSCRIPT || kind == TK_HTML || kind == TK_COMMAND) && !pTestVariant->testInfo.hasData[TIK_BASELINE]) {
        if (!CheckForPass(full, optReportBuf, cmdbuf, fDumpOutputFile)) {
            fFailed = TRUE;
            goto SkipLogFailure;
        }
    }

logFailure:
    if (fFailed) {
        LogOut("ERROR: Test failed to run correctly: %s (%s):",
            reason, optReportBuf);
        LogOut("    %s", cmdbuf);
        if (fDumpOutputFile) {
            DumpFileToLog(full);
        }
    }

SkipLogFailure:
    if (fFileToDelete && !FNoDelete) {
        DeleteFileRetryMsg(full);
    }

    elapsed_variation = (int)(time(NULL) - start_variation);
    if (Timing & TIME_VARIATION) {
        Message("RL: Variation elapsed time (%s, %s, %s): %02d:%02d",
            pDir->GetDirectoryName(),
            kind == TK_MAKEFILE ? "rl.mak" : "dotest.cmd",
            optReportBuf,
            elapsed_variation / 60, elapsed_variation % 60);
    }

    if (kind == TK_MAKEFILE) {

        // If the test failed and we are asked to copy the failures, do so.

        if (fFailed && FCopyOnFail) {
            sprintf_s(cmdbuf, NMAKE"%s copy COPYDIR=\"fail.%s.%s\"",
                testCmd, optFlags, linkFlags);
            Message(cmdbuf);
            ExecuteCommand(pDir->GetDirectoryPath(), cmdbuf);
        }

        // Clean up after ourselves.

        if (!FNoDelete && (fFailed || fCleanAfter)) {
            sprintf_s(cmdbuf, NMAKE"%s clean", testCmd);
            Message(cmdbuf);
            ExecuteCommand(pDir->GetDirectoryPath(), cmdbuf);
        }
    }

    if (FSyncVariation) {
        if (FRLFE && fFailed) {
            RLFEAddLog(pDir, RLFES_FAILED, testCmd,
                optReportBuf, ThreadOut->GetText());
        }

        if (fSyncVariationWhenFinished)
            FlushOutput();
    }
    DeleteFileIfFound(full);

    return fFailed ? -1 : 0;
}

// null terminated string of null terminated strings
// name=data from testinfo and all of parent process env, arg for CreateProcess()
void * GetEnvFlags
    (
    TestVariant * pTestVariant
    )
{
    char temp[BUFFER_SIZE];
    size_t len = 0, totalEnvLen = 0;
    char * envFlags = NULL;
    Xml::Node *env = (Xml::Node *)pTestVariant->testInfo.data[TIK_ENV];
    if (env != NULL) {
        // use a fixed global array for memory
        memset(EnvFlags, '\0', MAX_ENV_LEN);
        *temp = '\0';
        for ( Xml::Node * child = env->ChildList; child != NULL; child = child->Next) {
            sprintf_s(temp, "%s=%s", child->Name, child->Data);
            if (envFlags == NULL) {
                sprintf_s(EnvFlags, "%s", temp);
                envFlags = EnvFlags;
            } else {
                strcat_s(envFlags, REMAININGARRAYLEN(EnvFlags, envFlags), temp);
            }
            len = strlen(envFlags);
            envFlags += len+1;
        }
        LPTSTR lpszParentEnv = GetEnvironmentStrings();
        totalEnvLen = len;
        ASSERT(totalEnvLen < BUFFER_SIZE);
        len = 0;
        while(!((lpszParentEnv[len] == '\0') && (lpszParentEnv[len+1] == '\0'))) {
            len++;
        }
        ASSERT(totalEnvLen+len+2 < MAX_ENV_LEN);
        memcpy(envFlags, lpszParentEnv, len+2);
        FreeEnvironmentStrings(lpszParentEnv);
        envFlags = EnvFlags;
    }
    return (void*)envFlags;
}

int
    DoExternalTest(
    CDirectory* pDir,
    TestVariant * pTestVariant,
    char *testCmd,
    ExternalTestKind kind,
    BOOL fSuppressNoGPF,
    DWORD millisecTimeout
    )
{
    char *ccFlags = pTestVariant->testInfo.data[TIK_COMPILE_FLAGS];
    void *envFlags = GetEnvFlags(pTestVariant);
    return DoOneExternalTest(pDir, pTestVariant, pTestVariant->optFlags, ccFlags, NULL,
        testCmd, kind, TRUE, TRUE, TRUE, fSuppressNoGPF, envFlags, millisecTimeout);
}

int
    DoPogoExternalTest(
    CDirectory* pDir,
    TestVariant * pTestVariant,
    char *testCmd,
    ExternalTestKind kind,
    BOOL fSuppressNoGPF,
    DWORD millisecTimeout
    )
{
    static char *pgc = "*.pgc";
    static char *pgd = POGO_PGD;
    char pgdFull[MAX_PATH];
    char ccFlags[BUFFER_SIZE];
    char linkFlags[BUFFER_SIZE];
    char cmdbuf[BUFFER_SIZE];
    BOOL fFailed;
    void *envFlags = GetEnvFlags(pTestVariant);

    sprintf_s(pgdFull, "%s\\%s", pDir->GetDirectoryPath(), pgd);

    char * inCCFlags = pTestVariant->testInfo.data[TIK_COMPILE_FLAGS];
    char * optFlags = pTestVariant->optFlags;

    DeleteFileIfFound(pgdFull);
    DeleteMultipleFiles(pDir, pgc);
    fFailed = FALSE;

    // Pogo requires LTCG

    ASSERT(strstr(optFlags, "GL") != NULL);

    if (!kind == TK_MAKEFILE) {
        Warning("'%s\\%s' is not a makefile test; Pogo almost certainly won't work", pDir->GetDirectoryPath(), testCmd);
    }

    sprintf_s(ccFlags, "%s %s", PogoForceErrors, optFlags);
    sprintf_s(linkFlags, "-ltcg:pgi -pgd:%s", pgd);

    if (DoOneExternalTest(pDir, pTestVariant, ccFlags, inCCFlags, linkFlags,
        testCmd, kind, FALSE, TRUE, FALSE, fSuppressNoGPF, envFlags, millisecTimeout)) {
            fFailed = TRUE;
            goto logFailure;
    }

    sprintf_s(ccFlags, "%s %s", PogoForceErrors, optFlags);
    sprintf_s(linkFlags, "-ltcg:pgo -pgd:%s", pgd);

    // Manually erase EXE and DLL files to get makefile to relink.
    // Also erase ASM files because some makefiles try to rebuild from
    // them.

    DeleteMultipleFiles(pDir, "*.exe");
    DeleteMultipleFiles(pDir, "*.dll");
    DeleteMultipleFiles(pDir, "*.asm");

    if (DoOneExternalTest(pDir, pTestVariant, ccFlags, inCCFlags, linkFlags,
        testCmd, kind, FALSE, FALSE, TRUE, fSuppressNoGPF, envFlags, millisecTimeout)) {
            fFailed = TRUE;
    }

logFailure:

    if (FSyncVariation) {
        if (FRLFE && fFailed) {
            sprintf_s(cmdbuf, "%s%s%s", ccFlags,
                *linkFlags ? ";" : "", linkFlags);
            RLFEAddLog(pDir, RLFES_FAILED, testCmd,
                cmdbuf, ThreadOut->GetText());
        }

        FlushOutput();
    }

    if (FRLFE)
        RLFETestStatus(pDir);

    if (!FNoDelete) {
        DeleteFileRetryMsg(pgdFull);
        DeleteMultipleFiles(pDir, pgc);
    }

    return fFailed ? -1 : 0;
}

BOOL
    DoOneSimpleTest(
    CDirectory *pDir,
    Test * pTest,
    TestVariant * pTestVariant,
    char *optFlags,
    char *inCCFlags,
    char *inLinkFlags,
    BOOL fSyncVariationWhenFinished,
    BOOL fCleanAfter,
    BOOL fLinkOnly,    // relink only
    BOOL fSuppressNoGPF,
    DWORD millisecTimeout
    )
{
    int rc;
    char *p = NULL;
    char cmdbuf[BUFFER_SIZE*2];
    char ccFlags[BUFFER_SIZE];
    char linkFlags[BUFFER_SIZE];
    char nogpfFlags[BUFFER_SIZE];
    char optReportBuf[BUFFER_SIZE];
    char full[MAX_PATH];
    char exebuf[BUFFER_SIZE];
    char fullexebuf[BUFFER_SIZE];
    char buf[BUFFER_SIZE];
    char failDir[BUFFER_SIZE];
    char copyName[BUFFER_SIZE];
    char tmp_file1[MAX_PATH];
    char tmp_file2[MAX_PATH];
    time_t start_variation;
    UINT elapsed_variation;
    BOOL fFailed;
    void *envFlags = GetEnvFlags(pTestVariant);

    // Avoid conditionals by copying/creating ccFlags appropriately.

    if (inCCFlags)
        sprintf_s(ccFlags, " %s", inCCFlags);
    else
        ccFlags[0] = '\0';

    switch (TargetMachine) {
    case TM_WVM:
    case TM_WVMX86:
    case TM_WVM64:
        strcat_s(ccFlags, " /BC ");
        break;
    }

    if (inLinkFlags)
        strcpy_s(linkFlags, inLinkFlags);
    else
        linkFlags[0] = '\0';

    sprintf_s(optReportBuf, "%s%s%s", optFlags, *linkFlags ? ";" : "", linkFlags);

    // Figure out the exe name and path

    strcpy_s(exebuf, pTest->name);
    p = strrchr(exebuf, '.');
    if (p != NULL)
    {
        strcpy_s(p + 1, REMAININGARRAYLEN(exebuf, p + 1), "exe");
    }
    else
    {
        strcat_s(exebuf, ".exe");
    }

    sprintf_s(fullexebuf, "%s\\%s", pDir->GetDirectoryPath(), exebuf);

    start_variation = time(NULL);

    // Build up the compile command string.

    sprintf_s(cmdbuf, "%s %s%s %s", REGR_CL,
        optFlags, ccFlags, EXTRA_CC_FLAGS);

    for (StringList * pFile = pTest->files; pFile != NULL; pFile = pFile->next)
    {
        strcat_s(cmdbuf, " ");
        strcat_s(cmdbuf, pFile->string);

        // If we're only relinking, hammer the extension to .obj

        if (fLinkOnly)
        {
            p = strrchr(cmdbuf, '.');
            sprintf_s(p, REMAININGARRAYLEN(cmdbuf, p), ".obj");
        }
    }

    // Build the link option string.

    if (LINKFLAGS && LINKFLAGS[0] != '\0') {
        strcat_s(linkFlags, " ");
        strcat_s(linkFlags, LINKFLAGS);
    }

    FillNoGPFFlags(nogpfFlags, fSuppressNoGPF);
    strcat_s(linkFlags, nogpfFlags);

    switch (TargetMachine) {
    case TM_X86:
    case TM_IA64:
    case TM_AMD64:
    case TM_AMD64SYS:
    case TM_AM33:
    case TM_ARM:
    case TM_ARM64:
    case TM_THUMB:
    case TM_M32R:
    case TM_MIPS:
    case TM_SH3:
    case TM_SH4:
    case TM_SH5M:
    case TM_SH5C:
    case TM_WVMX86:
        if (*linkFlags) {
            strcat_s(cmdbuf, " /link ");
            strcat_s(cmdbuf, linkFlags);
        }
        break;
    case TM_WVM:
        strcat_s(cmdbuf, " /c ");
        break;
    case TM_PPCWCE:
        if (*linkFlags) {
            strcat_s(cmdbuf, " ");
            strcat_s(cmdbuf, linkFlags);
        }
        break;
    }

    sprintf_s(buf, "%s (%s)", pTest->name, optReportBuf);
    ThreadInfo[ThreadId].SetCurrentTest(pDir->GetDirectoryName(), buf, pDir->IsBaseline());
    UpdateTitleStatus();

    // Remove exe if it's already there. We have to keep trying to delete it
    // until it's gone, or else the link will fail (if it is somehow still in
    // use).

    DeleteFileRetryMsg(fullexebuf);

    if (FTest)
    {
        Message("%s", cmdbuf);
        if (pTestVariant->testInfo.data[TIK_BASELINE]) {
            Message("   (baseline %s)", pTestVariant->testInfo.data[TIK_BASELINE]);
        }
        return 0;
    }

    // Do the compile.

    Message("Compiling:");
    Message("    %s", cmdbuf);

    rc = ExecuteCommand(pDir->GetDirectoryPath(), cmdbuf);

    // Some machines require separate linking of the
    // compiler and/or assembler output.

    if (rc == 0)
    {
        switch (TargetMachine)
        {
        case TM_WVM:
            // Build up the linker command string.

            strcpy_s(cmdbuf, LINKER);

            for (StringList * pFile = pTest->files;
                pFile != NULL;
                pFile = pFile->next)
            {
                strcat_s(cmdbuf, " ");
                strcat_s(cmdbuf, pFile->string);
                p = strrchr(cmdbuf, '.');
                strcpy_s(p + 1, REMAININGARRAYLEN(cmdbuf, p + 1), "obj");
            }

            if (linkFlags) {
                strcat_s(cmdbuf, " ");
                strcat_s(cmdbuf, linkFlags);
            }

            // Do the link.

            Message("Linking:");
            Message("    %s", cmdbuf);
            ExecuteCommand(pDir->GetDirectoryPath(), cmdbuf);
            break;

        default:
            break;
        }
    }

    // See if the compile succeeded by checking for the existence
    // of the executable.

    if ((rc != 0) || GetFileAttributes(fullexebuf) == INVALID_FILE_ATTRIBUTES) {
        LogOut("ERROR: Test failed to compile or link (%s):", optReportBuf);
        LogOut("    %s", cmdbuf);
        fFailed = TRUE;
        goto logFailure;
    }

    // Run the resulting exe.

    if (TargetVM) {
        strcpy_s(buf, TargetVM);
        strcat_s(buf, " ");
        strcat_s(buf, exebuf);

        // Copy the VM command to cmdbuf, so we get a useful error message
        // in the log file if test fails.

        strcpy_s(cmdbuf, buf);
    }
    else {
        strcpy_s(buf, exebuf);
    }

    // We need some temporary files.
    // Note: these are full pathnames, not relative pathnames. Also, note that
    // mytmpnam creates the file to be used. To avoid losing that file, and
    // risking another program using it, don't delete the file before use.
    // We currently delete the file before running the test, so we can see if
    // the test ever creates it. We should probably create the temp files in
    // the same directory as the test, since we guarantee that no other copies
    // of RL are running in the same directory.

    if (mytmpnam(pDir->GetDirectoryPath(), TMP_PREFIX, tmp_file1) == NULL ||
        mytmpnam(pDir->GetDirectoryPath(), TMP_PREFIX, tmp_file2) == NULL) {
            Fatal("Unable to create temporary files");
    }

    ThreadInfo[ThreadId].AddToTmpFileList(tmp_file1);
    ThreadInfo[ThreadId].AddToTmpFileList(tmp_file2);

    if (FVerbose)
        Message("INFO: tmp file 1 = %s, tmp file 2 = %s", tmp_file1, tmp_file2);

    Message("Running the test (%s)", buf);
    strcat_s(buf, " > ");
    strcat_s(buf, tmp_file1);

    // Make sure the output file isn't there.

    DeleteFileIfFound(tmp_file1);

    int retval = ExecuteCommand(pDir->GetDirectoryPath(), buf, millisecTimeout, envFlags);

    fFailed = FALSE;

    // Check for timeout.

    if (retval == WAIT_TIMEOUT) {
        ASSERT(millisecTimeout != INFINITE);
        LogOut("ERROR: Test timed out after %ul seconds", millisecTimeout / 1000);
        fFailed = TRUE;
        goto logFailure;
    }

    // Check the output.

    if (pTestVariant->testInfo.data[TIK_BASELINE]) {
        int spiff_ret;

        // Check to see if the exe ran at all.

        if (GetFileAttributes(tmp_file1) == INVALID_FILE_ATTRIBUTES) {
            LogOut("ERROR: Test failed to run. Couldn't find file '%s' (%s):", tmp_file1, optReportBuf);
            LogOut("    %s", cmdbuf);
            fFailed = TRUE;
        }
        else {
            sprintf_s(full, "%s\\%s", pDir->GetDirectoryPath(),
                pTestVariant->testInfo.data[TIK_BASELINE]);
            if (DoCompare(tmp_file1, full)) {

                // Output differs, run spiff to see if it's just minor
                // floating point anomalies.

                DeleteFileIfFound(tmp_file2);
                sprintf_s(buf, "spiff -m -n -s \"command spiff\" %s %s > %s",
                    tmp_file1, full, tmp_file2);
                spiff_ret = ExecuteCommand(pDir->GetDirectoryPath(), buf);
                if (GetFileAttributes(tmp_file2) == INVALID_FILE_ATTRIBUTES) {
                    LogError("ERROR: spiff failed to run");
                    fFailed = TRUE;
                }
                else if (spiff_ret) {
                    LogOut("ERROR: Test failed to run correctly. spiff returned %d (%s):", spiff_ret, optReportBuf);
                    LogOut("    %s", cmdbuf);
                    fFailed = TRUE;
                }
            }
        }
    }
    else {
        if (!CheckForPass(tmp_file1, optReportBuf, cmdbuf)) {
            fFailed = TRUE;
        }
    }

logFailure:

    if (fFailed) {
        if (FCopyOnFail) {
            if (FVerbose)
                Message("INFO: Copying '%s' failure", optReportBuf);

            sprintf_s(failDir, "%s\\fail.%s",
                pDir->GetDirectoryPath(), optReportBuf);

            if ((GetFileAttributes(failDir) == INVALID_FILE_ATTRIBUTES) &&
                !CreateDirectory(failDir, NULL)) {
                    Message("ERROR: Couldn't create directory '%s'", failDir);
            }
            else
            {
                for (StringList * pFile = pTest->files;
                    pFile != NULL;
                    pFile = pFile->next)
                {
                    sprintf_s(copyName, "%s\\%s", failDir, pFile->string);
                    p = strrchr(copyName, '.') + 1;
                    strcpy_s(p, REMAININGARRAYLEN(copyName, p + 1), "obj");
                    sprintf_s(buf, "%s\\%s", pDir->GetDirectoryPath(),
                        pFile->string);
                    p = strrchr(buf, '.') + 1;
                    strcpy_s(p, REMAININGARRAYLEN(buf, p + 1), "obj");

                    if (!CopyFile(buf, copyName, FALSE)) {
                        Message("ERROR: Couldn't copy '%s' to '%s'",
                            buf, copyName);
                    }
                }

                sprintf_s(copyName, "%s\\%s", failDir, exebuf);
                if (!CopyFile(fullexebuf, copyName, FALSE)) {
                    Message("ERROR: Couldn't copy '%s' to '%s'",
                        fullexebuf, copyName);
                }
            }
        }
    }

    if (FRLFE) {
        RLFETestStatus(pDir);
    }

    if (FVerbose)
        Message("INFO: cleaning up test run");

    // Remove the exe.

    if (!FNoDelete) {
        DeleteFileRetryMsg(fullexebuf);
    }

    // Don't trash fullexebuf!

    strcpy_s(buf, fullexebuf);

    p = strrchr(buf, '.') + 1;

    // Remove the pdb(s) (if it exists).

    strcpy_s(p, REMAININGARRAYLEN(buf, p), "pdb");
    DeleteFileIfFound(buf);
    DeleteMultipleFiles(pDir, "*.pdb");

    // Remove the ilk (if it exists).

    strcpy_s(p, REMAININGARRAYLEN(buf, p), "ilk");
    DeleteFileIfFound(buf);

    // Remove the objs.

    if (!FNoDelete)
    {
        for (StringList * pFile = pTest->files;
            pFile != NULL;
            pFile = pFile->next)
        {
            sprintf_s(buf, "%s\\%s", pDir->GetDirectoryPath(), pFile->string);
            p = strrchr(buf, '.') + 1;

            if (fCleanAfter)
            {
                strcpy_s(p, REMAININGARRAYLEN(buf, p), "obj");
                DeleteFileRetryMsg(buf);
            }

            if (REGR_ASM) {
                strcpy_s(p, REMAININGARRAYLEN(buf, p), "asm");
                DeleteFileRetryMsg(buf);
            }
        }
    }

    elapsed_variation = (int)(time(NULL) - start_variation);
    if (Timing & TIME_VARIATION) {
        Message("RL: Variation elapsed time (%s, %s, %s): %02d:%02d",
            pDir->GetDirectoryName(), pTest->name, optReportBuf,
            elapsed_variation / 60, elapsed_variation % 60);
    }

    if (FSyncVariation) {
        if (FRLFE && fFailed)
            RLFEAddLog(pDir, RLFES_FAILED, pTest->name, optReportBuf, ThreadOut->GetText());

        if (fSyncVariationWhenFinished)
            FlushOutput();
    }

    ThreadInfo[ThreadId].DeleteTmpFileList();

    return fFailed ? -1 : 0;
}

int
    DoSimpleTest(
    CDirectory *pDir,
    Test * pTest,
    TestVariant * pTestVariant,
    BOOL fSuppressNoGPF,
    DWORD millisecTimeout
    )
{
    return DoOneSimpleTest(pDir, pTest, pTestVariant, pTestVariant->optFlags,
        pTestVariant->testInfo.data[TIK_COMPILE_FLAGS],
        pTestVariant->testInfo.data[TIK_LINK_FLAGS],
        TRUE, TRUE, FALSE, fSuppressNoGPF, millisecTimeout);
}

int
    DoPogoSimpleTest(
    CDirectory *pDir,
    Test * pTest,
    TestVariant * pTestVariant,
    BOOL fSuppressNoGPF,
    DWORD millisecTimeout
    )
{
    static char *pgc = "*.pgc";
    static char *pgd = POGO_PGD;
    char pgdFull[MAX_PATH];
    char ccFlags[BUFFER_SIZE];
    char linkFlags[BUFFER_SIZE];
    BOOL fFailed;

    sprintf_s(pgdFull, "%s\\%s", pDir->GetDirectoryPath(), pgd);

    char * inCCFlags = pTestVariant->testInfo.data[TIK_COMPILE_FLAGS];
    char * optFlags = pTestVariant->optFlags;

    DeleteFileIfFound(pgdFull);
    DeleteMultipleFiles(pDir, pgc);
    fFailed = FALSE;

    // Pogo requires LTCG

    ASSERT(strstr(optFlags, "GL") != NULL);

    sprintf_s(ccFlags, "%s %s", PogoForceErrors, optFlags);
    sprintf_s(linkFlags, "-ltcg:pgi -pgd:%s", pgd);

    if (DoOneSimpleTest(pDir, pTest, pTestVariant, ccFlags, inCCFlags,
        linkFlags, FALSE, FALSE, FALSE, fSuppressNoGPF, millisecTimeout)) {
            fFailed = TRUE;
            goto logFailure;
    }

    if (FTest)
    {
        return 0;
    }

    sprintf_s(ccFlags, "%s %s", PogoForceErrors, optFlags);
    sprintf_s(linkFlags, "-ltcg:pgo -pgd:%s", pgd);

    if (DoOneSimpleTest(pDir, pTest, pTestVariant, ccFlags, inCCFlags,
        linkFlags, FALSE, TRUE, TRUE, fSuppressNoGPF, millisecTimeout)) {
            fFailed = TRUE;
    }

logFailure:

    if (FSyncVariation) {
#if 0
        if (FRLFE && fFailed) {
            sprintf_s(cmdbuf, "%s%s%s", ccFlags,
                *linkFlags ? ";" : "", linkFlags);
            RLFEAddLog(pDir, RLFES_FAILED, testCmd,
                cmdbuf, ThreadOut->GetText());
        }
#endif

        FlushOutput();
    }

    if (!FNoDelete) {
        DeleteFileRetryMsg(pgdFull);
        DeleteMultipleFiles(pDir, pgc);
    }

    return fFailed ? -1 : 0;
}

int
    ExecTest
    (
    CDirectory* pDir,
    Test * pTest,
    TestVariant * pTestVariant
    )
{
    char *p = NULL;
    char full[MAX_PATH];
    DWORD millisecTimeout = DEFAULT_TEST_TIMEOUT;
    char *strTimeout = pTestVariant->testInfo.data[TIK_TIMEOUT];

    if (strTimeout) {
        char *end;
        _set_errno(0);
        unsigned long secTimeout = strtoul(strTimeout, &end, 10);
        millisecTimeout = 1000 * secTimeout;
        // Validation has already occurred so this string should
        // parse fine and the value shouldn't overflow the DWORD.
        ASSERT(errno == 0 && *end == 0);
        ASSERT(millisecTimeout > secTimeout);
    }

    // Check to see if all of the files exist.

    for (StringList * pFile = pTest->files; pFile != NULL; pFile = pFile->next)
    {
        // Get a pointer to the filename sans path, if present.

        p = GetFilenamePtr(pFile->string);

        // If we have no pathname, use the current directory.

        if (p == pFile->string) {
            sprintf_s(full, "%s\\", pDir->GetDirectoryPath());
        }
        else {

            // Look for %REGRESS% specifier.

            if (!_strnicmp(pFile->string, "%REGRESS%",
                strlen("%REGRESS%"))) {

                    // Temporarily truncate the filename.

                    ASSERT(p[-1] == '\\');
                    p[-1] = '\0';
                    sprintf_s(full, "%s%s\\",
                        REGRESS, pFile->string + strlen("%REGRESS%"));
                    p[-1] = '\\';
            }
            else {
                *p = '\0';
            }
        }

        strcat_s(full, p);

        if (GetFileAttributes(full) == INVALID_FILE_ATTRIBUTES) {
            LogError("ERROR: '%s' does not exist", pFile->string);
            return -1;
        }
    }

    const char* ext = GetFilenameExt(p);

    // Special case dotest.cmd
    if (!_stricmp(p, "dotest.cmd")) {

        // We don't handle these yet.

        ASSERTNR(pTestVariant->testInfo.data[TIK_LINK_FLAGS] == NULL);

        if (IsPogoTest(pTest))
            return DoPogoExternalTest(pDir, pTestVariant, full, TK_CMDSCRIPT, TRUE, millisecTimeout);
        else
            return DoExternalTest(pDir, pTestVariant, full, TK_CMDSCRIPT, TRUE, millisecTimeout);
    }

    // Special case for standardized RL makefiles.
    else if (!_stricmp(p, "rl.mak")) {

        // We don't handle these yet.

        ASSERTNR(pTestVariant->testInfo.data[TIK_LINK_FLAGS] == NULL);

        if (IsPogoTest(pTest))
            return DoPogoExternalTest(pDir, pTestVariant, full, TK_MAKEFILE, FALSE, millisecTimeout);
        else
            return DoExternalTest(pDir, pTestVariant, full, TK_MAKEFILE, SuppressNoGPF(pTest), millisecTimeout);
    }

    // Special case for files ending with ".js", ".html", ".htm" (<command> dealt with separately)
    else if (pTestVariant->testInfo.data[TIK_COMMAND] == NULL
        && !_stricmp(ext, ".js"))
    {
        return DoExternalTest(pDir, pTestVariant, full, TK_JSCRIPT, FALSE, millisecTimeout);
    }
    else if (pTestVariant->testInfo.data[TIK_COMMAND] == NULL
        && (!_stricmp(ext, ".html") || !_stricmp(ext, ".htm")))
    {
        return DoExternalTest(pDir, pTestVariant, full, TK_HTML, FALSE, millisecTimeout);
    }

    // Special case for tests with a <command> argument
    else if (pTestVariant->testInfo.data[TIK_COMMAND] != NULL)
    {
        return DoExternalTest(pDir, pTestVariant, full, TK_COMMAND, FALSE, millisecTimeout);
    }

    // Non-scripted test.

    else {
        if (IsPogoTest(pTest)) {

            // We don't handle these yet.

            ASSERTNR(pTestVariant->testInfo.data[TIK_LINK_FLAGS] == NULL);

            return DoPogoSimpleTest(pDir, pTest, pTestVariant, FALSE, millisecTimeout);
        }
        else
        {
            return DoSimpleTest(pDir, pTest, pTestVariant, SuppressNoGPF(pTest), millisecTimeout);
        }
    }
}
