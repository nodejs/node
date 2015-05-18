
/****************************************************************************** 
*
*  file:  MultiSwitchArg.h
*
*  Copyright (c) 2003, Michael E. Smoot .
*  Copyright (c) 2004, Michael E. Smoot, Daniel Aarno.
*  Copyright (c) 2005, Michael E. Smoot, Daniel Aarno, Erik Zeek.
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


#ifndef TCLAP_MULTI_SWITCH_ARG_H
#define TCLAP_MULTI_SWITCH_ARG_H

#include <string>
#include <vector>

#include "SwitchArg.h"

namespace TCLAP {

/**
* A multiple switch argument.  If the switch is set on the command line, then
* the getValue method will return the number of times the switch appears.
*/
class MultiSwitchArg : public SwitchArg
{
	protected:

		/**
		 * The value of the switch.
		 */
		int _value;

		/**
		 * Used to support the reset() method so that ValueArg can be
		 * reset to their constructed value.
		 */
		int _default;

	public:

		/**
		 * MultiSwitchArg constructor.
		 * \param flag - The one character flag that identifies this
		 * argument on the command line.
		 * \param name - A one word name for the argument.  Can be
		 * used as a long flag on the command line.
		 * \param desc - A description of what the argument is for or
		 * does.
		 * \param init - Optional. The initial/default value of this Arg. 
		 * Defaults to 0.
		 * \param v - An optional visitor.  You probably should not
		 * use this unless you have a very good reason.
		 */
		MultiSwitchArg(const std::string& flag, 
				const std::string& name,
				const std::string& desc,
				int init = 0,
				Visitor* v = NULL);


		/**
		 * MultiSwitchArg constructor.
		 * \param flag - The one character flag that identifies this
		 * argument on the command line.
		 * \param name - A one word name for the argument.  Can be
		 * used as a long flag on the command line.
		 * \param desc - A description of what the argument is for or
		 * does.
		 * \param parser - A CmdLine parser object to add this Arg to
		 * \param init - Optional. The initial/default value of this Arg. 
		 * Defaults to 0.
		 * \param v - An optional visitor.  You probably should not
		 * use this unless you have a very good reason.
		 */
		MultiSwitchArg(const std::string& flag, 
				const std::string& name,
				const std::string& desc,
				CmdLineInterface& parser,
				int init = 0,
				Visitor* v = NULL);


		/**
		 * Handles the processing of the argument.
		 * This re-implements the SwitchArg version of this method to set the
		 * _value of the argument appropriately.
		 * \param i - Pointer the the current argument in the list.
		 * \param args - Mutable list of strings. Passed
		 * in from main().
		 */
		virtual bool processArg(int* i, std::vector<std::string>& args); 

		/**
		 * Returns int, the number of times the switch has been set.
		 */
		int getValue();

		/**
		 * Returns the shortID for this Arg.
		 */
		std::string shortID(const std::string& val) const;

		/**
		 * Returns the longID for this Arg.
		 */
		std::string longID(const std::string& val) const;
		
		void reset();

};

//////////////////////////////////////////////////////////////////////
//BEGIN MultiSwitchArg.cpp
//////////////////////////////////////////////////////////////////////
inline MultiSwitchArg::MultiSwitchArg(const std::string& flag,
					const std::string& name,
					const std::string& desc,
					int init,
					Visitor* v )
: SwitchArg(flag, name, desc, false, v),
_value( init ),
_default( init )
{ }

inline MultiSwitchArg::MultiSwitchArg(const std::string& flag,
					const std::string& name, 
					const std::string& desc, 
					CmdLineInterface& parser,
					int init,
					Visitor* v )
: SwitchArg(flag, name, desc, false, v),
_value( init ),
_default( init )
{ 
	parser.add( this );
}

inline int MultiSwitchArg::getValue() { return _value; }

inline bool MultiSwitchArg::processArg(int *i, std::vector<std::string>& args)
{
	if ( _ignoreable && Arg::ignoreRest() )
		return false;

	if ( argMatches( args[*i] ))
	{
		// so the isSet() method will work
		_alreadySet = true;

		// Matched argument: increment value.
		++_value;

		_checkWithVisitor();

		return true;
	}
	else if ( combinedSwitchesMatch( args[*i] ) )
	{
		// so the isSet() method will work
		_alreadySet = true;

		// Matched argument: increment value.
		++_value;

		// Check for more in argument and increment value.
		while ( combinedSwitchesMatch( args[*i] ) ) 
			++_value;

		_checkWithVisitor();

		return false;
	}
	else
		return false;
}

inline std::string 
MultiSwitchArg::shortID(const std::string& val) const
{
	return Arg::shortID(val) + " ... ";
}

inline std::string 
MultiSwitchArg::longID(const std::string& val) const
{
	return Arg::longID(val) + "  (accepted multiple times)";
}

inline void
MultiSwitchArg::reset()
{
	MultiSwitchArg::_value = MultiSwitchArg::_default;
}

//////////////////////////////////////////////////////////////////////
//END MultiSwitchArg.cpp
//////////////////////////////////////////////////////////////////////

} //namespace TCLAP

#endif
