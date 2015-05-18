// -*- Mode: c++; c-basic-offset: 4; tab-width: 4; -*-

/****************************************************************************** 
 * 
 *  file:  ArgException.h
 * 
 *  Copyright (c) 2003, Michael E. Smoot .
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


#ifndef TCLAP_ARG_EXCEPTION_H
#define TCLAP_ARG_EXCEPTION_H

#include <string>
#include <exception>

namespace TCLAP {

/**
 * A simple class that defines and argument exception.  Should be caught
 * whenever a CmdLine is created and parsed.
 */
class ArgException : public std::exception
{
	public:
	
		/**
		 * Constructor.
		 * \param text - The text of the exception.
		 * \param id - The text identifying the argument source.
		 * \param td - Text describing the type of ArgException it is.
		 * of the exception.
		 */
		ArgException( const std::string& text = "undefined exception", 
					  const std::string& id = "undefined",
					  const std::string& td = "Generic ArgException")
			: std::exception(), 
			  _errorText(text), 
			  _argId( id ), 
			  _typeDescription(td)
		{ } 
		
		/**
		 * Destructor.
		 */
		virtual ~ArgException() throw() { }

		/**
		 * Returns the error text.
		 */
		std::string error() const { return ( _errorText ); }

		/**
		 * Returns the argument id.
		 */
		std::string argId() const  
		{ 
			if ( _argId == "undefined" )
				return " ";
			else
				return ( "Argument: " + _argId ); 
		}

		/**
		 * Returns the arg id and error text. 
		 */
		const char* what() const throw() 
		{
			static std::string ex; 
			ex = _argId + " -- " + _errorText;
			return ex.c_str();
		}

		/**
		 * Returns the type of the exception.  Used to explain and distinguish
		 * between different child exceptions.
		 */
		std::string typeDescription() const
		{
			return _typeDescription; 
		}


	private:

		/**
		 * The text of the exception message.
		 */
		std::string _errorText;

		/**
		 * The argument related to this exception.
		 */
		std::string _argId;

		/**
		 * Describes the type of the exception.  Used to distinguish
		 * between different child exceptions.
		 */
		std::string _typeDescription;

};

/**
 * Thrown from within the child Arg classes when it fails to properly
 * parse the argument it has been passed.
 */
class ArgParseException : public ArgException
{ 
	public:
		/**
		 * Constructor.
		 * \param text - The text of the exception.
		 * \param id - The text identifying the argument source 
		 * of the exception.
		 */
		ArgParseException( const std::string& text = "undefined exception", 
					       const std::string& id = "undefined" )
			: ArgException( text, 
			                id, 
							std::string( "Exception found while parsing " ) + 
							std::string( "the value the Arg has been passed." ))
			{ }
};

/**
 * Thrown from CmdLine when the arguments on the command line are not
 * properly specified, e.g. too many arguments, required argument missing, etc.
 */
class CmdLineParseException : public ArgException
{
	public:
		/**
		 * Constructor.
		 * \param text - The text of the exception.
		 * \param id - The text identifying the argument source 
		 * of the exception.
		 */
		CmdLineParseException( const std::string& text = "undefined exception", 
					           const std::string& id = "undefined" )
			: ArgException( text, 
			                id,
							std::string( "Exception found when the values ") +
							std::string( "on the command line do not meet ") +
							std::string( "the requirements of the defined ") +
							std::string( "Args." ))
		{ }
};

/**
 * Thrown from Arg and CmdLine when an Arg is improperly specified, e.g. 
 * same flag as another Arg, same name, etc.
 */
class SpecificationException : public ArgException
{
	public:
		/**
		 * Constructor.
		 * \param text - The text of the exception.
		 * \param id - The text identifying the argument source 
		 * of the exception.
		 */
		SpecificationException( const std::string& text = "undefined exception",
					            const std::string& id = "undefined" )
			: ArgException( text, 
			                id,
							std::string("Exception found when an Arg object ")+
							std::string("is improperly defined by the ") +
							std::string("developer." )) 
		{ }

};

class ExitException {
public:
	ExitException(int estat) : _estat(estat) {}

	int getExitStatus() const { return _estat; }

private:
	int _estat;
};

} // namespace TCLAP

#endif

