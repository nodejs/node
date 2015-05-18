

/****************************************************************************** 
 * 
 *  file:  CmdLineOutput.h
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

#ifndef TCLAP_CMDLINEOUTPUT_H
#define TCLAP_CMDLINEOUTPUT_H

#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace TCLAP {

class CmdLineInterface;
class ArgException;

/**
 * The interface that any output object must implement.
 */
class CmdLineOutput 
{

	public:

		/**
		 * Virtual destructor.
		 */
		virtual ~CmdLineOutput() {}

		/**
		 * Generates some sort of output for the USAGE. 
		 * \param c - The CmdLine object the output is generated for. 
		 */
		virtual void usage(CmdLineInterface& c)=0;

		/**
		 * Generates some sort of output for the version. 
		 * \param c - The CmdLine object the output is generated for. 
		 */
		virtual void version(CmdLineInterface& c)=0;

		/**
		 * Generates some sort of output for a failure. 
		 * \param c - The CmdLine object the output is generated for. 
		 * \param e - The ArgException that caused the failure. 
		 */
		virtual void failure( CmdLineInterface& c, 
						      ArgException& e )=0;

};

} //namespace TCLAP
#endif 
