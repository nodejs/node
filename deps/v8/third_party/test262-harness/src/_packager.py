# Copyright (c) 2012 Ecma International.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

#--Imports---------------------------------------------------------------------
import argparse
import os
import sys
import xml.dom.minidom
import base64
import datetime
import shutil
import re
import json
import stat

from _common import convertDocString

#--Stubs-----------------------------------------------------------------------
def generateHarness(harnessType, jsonFile, description):
    pass


#------------------------------------------------------------------------------
from _packagerConfig import *

#--Globals---------------------------------------------------------------------

__parser = argparse.ArgumentParser(description= \
                                   'Tool used to generate the test262 website')
__parser.add_argument('version', action='store',
                      help='Version of the test suite.')
__parser.add_argument('--type', action='store', default=DEFAULT_TESTCASE_TEMPLATE,
                      help='Type of test case runner to generate.')
__parser.add_argument('--console', action='store_true', default=False,
                      help='Type of test case runner to generate.')
ARGS = __parser.parse_args()

if not os.path.exists(EXCLUDED_FILENAME):
    print "Cannot generate (JSON) test262 tests without a file," + \
        " %s, showing which tests have been disabled!" % EXCLUDED_FILENAME
    sys.exit(1)
EXCLUDE_LIST = xml.dom.minidom.parse(EXCLUDED_FILENAME)
EXCLUDE_LIST = EXCLUDE_LIST.getElementsByTagName("test")
EXCLUDE_LIST = [x.getAttribute("id") for x in EXCLUDE_LIST]

#a list of all ES5 test chapter directories
TEST_SUITE_SECTIONS = []

#total number of tests accross the entire set of tests.
TOTAL_TEST_COUNT = 0

#List of all *.json files containing encoded test cases
SECTIONS_LIST = []


#--Sanity checks--------------------------------------------------------------#
if not os.path.exists(TEST262_CASES_DIR):
    print "Cannot generate (JSON) test262 tests when the path containing said tests, %s, does not exist!" % TEST262_CASES_DIR
    sys.exit(1)

if not os.path.exists(TEST262_HARNESS_DIR):
    print "Cannot copy the test harness from a path, %s, that does not exist!" % TEST262_HARNESS_DIR
    sys.exit(1)

if not os.path.exists(TEST262_WEB_CASES_DIR):
    os.mkdir(TEST262_WEB_CASES_DIR)

if not os.path.exists(TEST262_WEB_HARNESS_DIR):
    os.mkdir(TEST262_WEB_HARNESS_DIR)

if not hasattr(ARGS, "version"):
    print "A test262 suite version must be specified from the command-line to run this script!"
    sys.exit(1)

#--Helpers--------------------------------------------------------------------#
def createDepDirs(dirName):
    #base case
    if dirName==os.path.dirname(dirName):
        if not os.path.exists(dirName):
            os.mkdir(dirName)
    else:
        if not os.path.exists(dirName):
            createDepDirs(os.path.dirname(dirName))
            os.mkdir(dirName)

def test262PathToConsoleFile(path):
    stuff = os.path.join(TEST262_CONSOLE_CASES_DIR,
                         path.replace("/", os.path.sep))
    createDepDirs(os.path.dirname(stuff))
    return stuff

def getJSCount(dirName):
    '''
    Returns the total number of *.js files (recursively) under a given
    directory, dirName.
    '''
    retVal = 0
    if os.path.isfile(dirName) and dirName.endswith(".js"):
        retVal = 1
    elif os.path.isdir(dirName):
        tempList = [os.path.join(dirName, x) for x in os.listdir(dirName)]
        for x in tempList:
            retVal += getJSCount(x)
    #else:
    #    raise Exception("getJSCount: encountered a non-file/non-dir!")
    return retVal

#------------------------------------------------------------------------------
def dirWalker(dirName):
    '''
    Populates TEST_SUITE_SECTIONS with ES5 test directories based
    upon the number of test files per directory.
    '''
    global TEST_SUITE_SECTIONS
    #First check to see if it has test files directly inside it
    temp = [os.path.join(dirName, x) for x in os.listdir(dirName) \
                if not os.path.isdir(os.path.join(dirName, x))]
    if len(temp)!=0:
        TEST_SUITE_SECTIONS.append(dirName)
        return

    #Next check to see if all *.js files under this directory exceed our max
    #for a JSON file
    temp = getJSCount(dirName)
    if temp==0:
        print "ERROR:  expected there to be JavaScript tests under dirName!"
        sys.exit(1)
    #TODO - commenting out this elif/else clause seems to be causing *.json
    #naming conflicts WRT Sputnik test dirs.
    # elif temp < MAX_CASES_PER_JSON:
    TEST_SUITE_SECTIONS.append(dirName)
    return
    #TODO else:
    #    #Max has been exceeded.  We need to look at each subdir individually
    #    temp = os.listdir(dirName)
    #    for tempSubdir in temp:
    #        dirWalker(os.path.join(dirName, tempSubdir))

#------------------------------------------------------------------------------
def isTestStarted(line):
    '''
    Used to detect if we've gone past extraneous test comments in a test case.

    Note this is a naive approach on the sense that "/*abc*/" could be on one
    line.  However, we know for a fact this is not the case in IE Test Center
    or Sputnik tests.
    '''
    if re.search("^\s*//", line)!=None:     #//blah
        return False
    elif ("//" in line) and ("Copyright " in line):
        #BOM hack
        return False
    elif re.match("^\s*$", line)!=None: #newlines
        return False
    return True

#------------------------------------------------------------------------------
def getAllJSFiles(dirName):
    retVal = []
    for fullPath,dontCare,files in os.walk(dirName):
        retVal += [os.path.join(fullPath,b) for b in files if b.endswith(".js")]
    return retVal

#--MAIN------------------------------------------------------------------------
for temp in os.listdir(TEST262_CASES_DIR):
    temp = os.path.join(TEST262_CASES_DIR, temp)
    if not os.path.exists(temp):
        print "The expected ES5 test directory,", temp, "did not exist!"
        sys.exit(1)

    if temp.find("/.") != -1:
        # skip hidden files on Unix, such as ".DS_Store" on Mac
        continue

    if not ONE_JSON_PER_CHAPTER:
        dirWalker(temp)
    else:
        TEST_SUITE_SECTIONS.append(temp)

for chapter in TEST_SUITE_SECTIONS:
    chapterName = chapter.rsplit(os.path.sep, 1)[1]
    print "Generating test cases for ES5 chapter:", chapterName
    #create dictionaries for all our tests and a section
    testsList = {}
    sect = {}
    sect["name"] = "Chapter - " + chapterName

    #create an array for tests in a chapter
    tests = []
    sourceFiles = getAllJSFiles(chapter)

    if len(sourceFiles)!=0:
        excluded = 0
        testCount = 0
        for test in sourceFiles:
            #TODO - use something other than the hard-coded 'TestCases' below
            testPath =  "TestCases" + \
                test.split(TEST262_CASES_DIR, 1)[1].replace("\\", "/")
            testName=test.rsplit(".", 1)[0]
            testName=testName.rsplit(os.path.sep, 1)[1]
            if EXCLUDE_LIST.count(testName)==0:
                # dictionary for each test
                testDict = {}
                testDict["path"] = testPath

                tempFile = open(test, "rb")
                scriptCode = tempFile.readlines()
                tempFile.close()
                scriptCodeContent=""
                #Rip out license headers that add unnecessary bytes to
                #the JSON'ized test cases
                inBeginning = True

                #Hack to preserve the BOM
                if "Copyright " in scriptCode[0]:
                    scriptCodeContent += scriptCode[0]
                for line in scriptCode:
                    if inBeginning:
                        isStarted = isTestStarted(line)
                        if not isStarted:
                            continue
                        inBeginning = False
                    scriptCodeContent += line

                if scriptCodeContent==scriptCode[0]:
                    print "WARNING (" + test + \
                        "): unable to strip comments/license header/etc."
                    scriptCodeContent = "".join(scriptCode)
                scriptCodeContentB64 = base64.b64encode(scriptCodeContent)

                #add the test encoded code node to our test dictionary
                testDict["code"] = scriptCodeContentB64
                #now close the dictionary for the test

                #now get the metadata added.
                tempDict = convertDocString("".join(scriptCode))
                for tempKey in tempDict.keys():
                    #path is set from the file path above; the "@path" property
                    #in comments is redundant
                    if not (tempKey in ["path"]):
                        testDict[tempKey] = tempDict[tempKey]

                #this adds the test to our tests array
                tests.append(testDict)

                if ARGS.console:
                    with open(test262PathToConsoleFile(testDict["path"]),
                                  "w") as fConsole:
                        fConsole.write(scriptCodeContent)
                    with open(test262PathToConsoleFile(testDict["path"][:-3] + \
                                                       "_metadata.js"),
                              "w") as fConsoleMeta:
                        metaDict = testDict.copy()
                        del metaDict["code"]
                        fConsoleMeta.write("testDescrip = " + str(metaDict))
                testCount += 1
            else:
                print "Excluded:", testName
                excluded = excluded + 1

        #we have completed our tests
        # add section node, number of tests and the tests themselves.
        sect["numTests"] = str(len(sourceFiles)-excluded)
        sect["tests"] = tests

        #create a node for the tests and add it to our testsLists
        testsList["testsCollection"] = sect
        with open(os.path.join(TEST262_WEB_CASES_DIR, chapterName + ".json"),
                  "w") as f:
            json.dump(testsList, f, separators=(',',':'), sort_keys=True,
                      indent=0)


        if TESTCASELIST_PER_JSON:
            CHAPTER_TEST_CASES_JSON = {}
            CHAPTER_TEST_CASES_JSON["numTests"] = int(sect["numTests"])
            CHAPTER_TEST_CASES_JSON["testSuite"] = \
                [WEBSITE_CASES_PATH + chapterName + ".json"]
            with open(os.path.join(TEST262_WEB_CASES_DIR,
                                   "testcases_%s.json" % chapterName),
                      "w") as f:
                json.dump(CHAPTER_TEST_CASES_JSON, f, separators=(',',':'),
                          sort_keys=True, indent=0)
            generateHarness(ARGS.type, "testcases_%s.json" % chapterName,
                            chapterName.replace("ch", "Chapter "))

        #add the name of the chapter test to our complete list
        tempBool = True
        for tempRe in WEBSITE_EXCLUDE_RE_LIST:
            if tempRe.search(chapterName)!=None:
                tempBool = False
        if tempBool:
            SECTIONS_LIST.append(WEBSITE_CASES_PATH + chapterName + ".json")
            TOTAL_TEST_COUNT += int(sect["numTests"])


#we now have the list of files for each chapter
#create a root node for our suite
TEST_CASES_JSON = {}
TEST_CASES_JSON["numTests"] = TOTAL_TEST_COUNT
TEST_CASES_JSON["testSuite"] = SECTIONS_LIST
with open(os.path.join(TEST262_WEB_CASES_DIR, "default.json"), "w") as f:
    json.dump(TEST_CASES_JSON, f, separators=(',',':'), sort_keys=True, indent=0)
generateHarness(ARGS.type, "default.json", "Chapters 1-16")

#Overall description of this version of the test suite
SUITE_DESCRIP_JSON = {}
SUITE_DESCRIP_JSON["version"] = ARGS.version
SUITE_DESCRIP_JSON["date"] = str(datetime.datetime.now().date())
with open(os.path.join(TEST262_WEB_CASES_DIR, "suiteDescrip.json"), "w") as f:
    json.dump(SUITE_DESCRIP_JSON, f, separators=(',',':'), sort_keys=True)

#Deploy test harness to website as well
print ""
print "Deploying test harness files to 'TEST262_WEB_HARNESS_DIR'..."
if TEST262_HARNESS_DIR!=TEST262_WEB_HARNESS_DIR:
    for filename in [x for x in os.listdir(TEST262_HARNESS_DIR) \
                         if x.endswith(".js")]:
        toFilenameList = [ os.path.join(TEST262_WEB_HARNESS_DIR, filename)]
        if ARGS.console:
            toFilenameList.append(os.path.join(TEST262_CONSOLE_HARNESS_DIR,
                                               filename))

        for toFilename in toFilenameList:
            if not os.path.exists(os.path.dirname(toFilename)):
                os.mkdir(os.path.dirname(toFilename))
            fileExists = os.path.exists(toFilename)
            if fileExists:
                SC_HELPER.edit(toFilename)
            shutil.copy(os.path.join(TEST262_HARNESS_DIR, filename),
                        toFilename)
            if not fileExists:
                SC_HELPER.add(toFilename)

print "Done."
