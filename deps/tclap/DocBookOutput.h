// -*- Mode: c++; c-basic-offset: 4; tab-width: 4; -*-

/****************************************************************************** 
 * 
 *  file:  DocBookOutput.h
 * 
 *  Copyright (c) 2004, Michael E. Smoot
 *  All rights reverved.
 * 
 *  See the file COPYING in the top directory of this distribution for
 *  more information.
 *  
 *  THE SOFTWARE IS PROVIDED _AS IS_, WITHOUT WARRANTY OF ANY KIND, EXPRESS 
 *  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 *  DEALINGS IN THE SOFTWARE.  
 *  
 *****************************************************************************/ 

#ifndef TCLAP_DOCBOOKOUTPUT_H
#define TCLAP_DOCBOOKOUTPUT_H

#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <algorithm>

#include "CmdLineInterface.h"
#include "CmdLineOutput.h"
#include "XorHandler.h"
#include "Arg.h"

namespace TCLAP {

/**
 * A class that generates DocBook output for usage() method for the 
 * given CmdLine and its Args.
 */
class DocBookOutput : public CmdLineOutput
{

	public:

		/**
		 * Prints the usage to stdout.  Can be overridden to 
		 * produce alternative behavior.
		 * \param c - The CmdLine object the output is generated for. 
		 */
		virtual void usage(CmdLineInterface& c);

		/**
		 * Prints the version to stdout. Can be overridden 
		 * to produce alternative behavior.
		 * \param c - The CmdLine object the output is generated for. 
		 */
		virtual void version(CmdLineInterface& c);

		/**
		 * Prints (to stderr) an error message, short usage 
		 * Can be overridden to produce alternative behavior.
		 * \param c - The CmdLine object the output is generated for. 
		 * \param e - The ArgException that caused the failure. 
		 */
		virtual void failure(CmdLineInterface& c, 
						     ArgException& e );

	    DocBookOutput() : theDelimiter('=') {}
	protected:

		/**
		 * Substitutes the char r for string x in string s.
		 * \param s - The string to operate on. 
		 * \param r - The char to replace. 
		 * \param x - What to replace r with. 
		 */
		void substituteSpecialChars( std::string& s, char r, std::string& x );
		void removeChar( std::string& s, char r);
		void basename( std::string& s );

		void printShortArg(Arg* it);
		void printLongArg(Arg* it);

		char theDelimiter;
};


inline void DocBookOutput::version(CmdLineInterface& _cmd) 
{ 
	std::cout << _cmd.getVersion() << std::endl;
}

inline void DocBookOutput::usage(CmdLineInterface& _cmd ) 
{
	std::list<Arg*> argList = _cmd.getArgList();
	std::string progName = _cmd.getProgramName();
	std::string xversion = _cmd.getVersion();
	theDelimiter = _cmd.getDelimiter();
	XorHandler xorHandler = _cmd.getXorHandler();
	std::vector< std::vector<Arg*> > xorList = xorHandler.getXorList();
	basename(progName);

	std::cout << "<?xml version='1.0'?>" << std::endl;
	std::cout << "<!DOCTYPE refentry PUBLIC \"-//OASIS//DTD DocBook XML V4.2//EN\"" << std::endl;
	std::cout << "\t\"http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd\">" << std::endl << std::endl;

	std::cout << "<refentry>" << std::endl;

	std::cout << "<refmeta>" << std::endl;
	std::cout << "<refentrytitle>" << progName << "</refentrytitle>" << std::endl;
	std::cout << "<manvolnum>1</manvolnum>" << std::endl;
	std::cout << "</refmeta>" << std::endl;

	std::cout << "<refnamediv>" << std::endl;
	std::cout << "<refname>" << progName << "</refname>" << std::endl;
	std::cout << "<refpurpose>" << _cmd.getMessage() << "</refpurpose>" << std::endl;
	std::cout << "</refnamediv>" << std::endl;

	std::cout << "<refsynopsisdiv>" << std::endl;
	std::cout << "<cmdsynopsis>" << std::endl;

	std::cout << "<command>" << progName << "</command>" << std::endl;

	// xor
	for ( int i = 0; (unsigned int)i < xorList.size(); i++ )
	{
		std::cout << "<group choice='req'>" << std::endl;
		for ( ArgVectorIterator it = xorList[i].begin(); 
						it != xorList[i].end(); it++ )
			printShortArg((*it));

		std::cout << "</group>" << std::endl;
	}
	
	// rest of args
	for (ArgListIterator it = argList.begin(); it != argList.end(); it++)
		if ( !xorHandler.contains( (*it) ) )
			printShortArg((*it));

 	std::cout << "</cmdsynopsis>" << std::endl;
	std::cout << "</refsynopsisdiv>" << std::endl;

	std::cout << "<refsect1>" << std::endl;
	std::cout << "<title>Description</title>" << std::endl;
	std::cout << "<para>" << std::endl;
	std::cout << _cmd.getMessage() << std::endl; 
	std::cout << "</para>" << std::endl;
	std::cout << "</refsect1>" << std::endl;

	std::cout << "<refsect1>" << std::endl;
	std::cout << "<title>Options</title>" << std::endl;

	std::cout << "<variablelist>" << std::endl;
	
	for (ArgListIterator it = argList.begin(); it != argList.end(); it++)
		printLongArg((*it));

	std::cout << "</variablelist>" << std::endl;
	std::cout << "</refsect1>" << std::endl;

	std::cout << "<refsect1>" << std::endl;
	std::cout << "<title>Version</title>" << std::endl;
	std::cout << "<para>" << std::endl;
	std::cout << xversion << std::endl; 
	std::cout << "</para>" << std::endl;
	std::cout << "</refsect1>" << std::endl;
	
	std::cout << "</refentry>" << std::endl;

}

inline void DocBookOutput::failure( CmdLineInterface& _cmd,
				    ArgException& e ) 
{ 
	static_cast<void>(_cmd); // unused
	std::cout << e.what() << std::endl;
	throw ExitException(1);
}

inline void DocBookOutput::substituteSpecialChars( std::string& s,
				                                   char r,
												   std::string& x )
{
	size_t p;
	while ( (p = s.find_first_of(r)) != std::string::npos )
	{
		s.erase(p,1);
		s.insert(p,x);
	}
}

inline void DocBookOutput::removeChar( std::string& s, char r)
{
	size_t p;
	while ( (p = s.find_first_of(r)) != std::string::npos )
	{
		s.erase(p,1);
	}
}

inline void DocBookOutput::basename( std::string& s )
{
	size_t p = s.find_last_of('/');
	if ( p != std::string::npos )
	{
		s.erase(0, p + 1);
	}
}

inline void DocBookOutput::printShortArg(Arg* a)
{
	std::string lt = "&lt;"; 
	std::string gt = "&gt;"; 

	std::string id = a->shortID();
	substituteSpecialChars(id,'<',lt);
	substituteSpecialChars(id,'>',gt);
	removeChar(id,'[');
	removeChar(id,']');
	
	std::string choice = "opt";
	if ( a->isRequired() )
		choice = "plain";

	std::cout << "<arg choice='" << choice << '\'';
	if ( a->acceptsMultipleValues() )
		std::cout << " rep='repeat'";


	std::cout << '>';
	if ( !a->getFlag().empty() )
		std::cout << a->flagStartChar() << a->getFlag();
	else
		std::cout << a->nameStartString() << a->getName();
	if ( a->isValueRequired() )
	{
		std::string arg = a->shortID();
		removeChar(arg,'[');
		removeChar(arg,']');
		removeChar(arg,'<');
		removeChar(arg,'>');
		arg.erase(0, arg.find_last_of(theDelimiter) + 1);
		std::cout << theDelimiter;
		std::cout << "<replaceable>" << arg << "</replaceable>";
	}
	std::cout << "</arg>" << std::endl;

}

inline void DocBookOutput::printLongArg(Arg* a)
{
	std::string lt = "&lt;"; 
	std::string gt = "&gt;"; 

	std::string desc = a->getDescription();
	substituteSpecialChars(desc,'<',lt);
	substituteSpecialChars(desc,'>',gt);

	std::cout << "<varlistentry>" << std::endl;

	if ( !a->getFlag().empty() )
	{
		std::cout << "<term>" << std::endl;
		std::cout << "<option>";
		std::cout << a->flagStartChar() << a->getFlag();
		std::cout << "</option>" << std::endl;
		std::cout << "</term>" << std::endl;
	}

	std::cout << "<term>" << std::endl;
	std::cout << "<option>";
	std::cout << a->nameStartString() << a->getName();
	if ( a->isValueRequired() )
	{
		std::string arg = a->shortID();
		removeChar(arg,'[');
		removeChar(arg,']');
		removeChar(arg,'<');
		removeChar(arg,'>');
		arg.erase(0, arg.find_last_of(theDelimiter) + 1);
		std::cout << theDelimiter;
		std::cout << "<replaceable>" << arg << "</replaceable>";
	}
	std::cout << "</option>" << std::endl;
	std::cout << "</term>" << std::endl;

	std::cout << "<listitem>" << std::endl;
	std::cout << "<para>" << std::endl;
	std::cout << desc << std::endl;
	std::cout << "</para>" << std::endl;
	std::cout << "</listitem>" << std::endl;

	std::cout << "</varlistentry>" << std::endl;
}

} //namespace TCLAP
#endif 
