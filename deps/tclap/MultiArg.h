/****************************************************************************** 
 * 
 *  file:  MultiArg.h
 * 
 *  Copyright (c) 2003, Michael E. Smoot .
 *  Copyright (c) 2004, Michael E. Smoot, Daniel Aarno.
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


#ifndef TCLAP_MULTIPLE_ARGUMENT_H
#define TCLAP_MULTIPLE_ARGUMENT_H

#include <string>
#include <vector>

#include "Arg.h"
#include "Constraint.h"

namespace TCLAP {
/**
 * An argument that allows multiple values of type T to be specified.  Very
 * similar to a ValueArg, except a vector of values will be returned
 * instead of just one.
 */
template<class T>
class MultiArg : public Arg
{
public:
	typedef std::vector<T> container_type;	
	typedef typename container_type::iterator iterator;
	typedef typename container_type::const_iterator const_iterator;

protected:

	/**
	 * The list of values parsed from the CmdLine.
	 */
	std::vector<T> _values;

	/**
	 * The description of type T to be used in the usage.
	 */
	std::string _typeDesc;

	/**
	 * A list of constraint on this Arg. 
	 */
	Constraint<T>* _constraint;

	/**
	 * Extracts the value from the string.
	 * Attempts to parse string as type T, if this fails an exception
	 * is thrown.
	 * \param val - The string to be read.
	 */
	void _extractValue( const std::string& val );

	/**
	 * Used by XorHandler to decide whether to keep parsing for this arg.
	 */
	bool _allowMore;

public:

	/**
	 * Constructor.
	 * \param flag - The one character flag that identifies this
	 * argument on the command line.
	 * \param name - A one word name for the argument.  Can be
	 * used as a long flag on the command line.
	 * \param desc - A description of what the argument is for or
	 * does.
	 * \param req - Whether the argument is required on the command
	 * line.
	 * \param typeDesc - A short, human readable description of the
	 * type that this object expects.  This is used in the generation
	 * of the USAGE statement.  The goal is to be helpful to the end user
	 * of the program.
	 * \param v - An optional visitor.  You probably should not
	 * use this unless you have a very good reason.
	 */
	MultiArg( const std::string& flag,
                  const std::string& name,
                  const std::string& desc,
                  bool req,
                  const std::string& typeDesc,
                  Visitor* v = NULL);

	/**
	 * Constructor.
	 * \param flag - The one character flag that identifies this
	 * argument on the command line.
	 * \param name - A one word name for the argument.  Can be
	 * used as a long flag on the command line.
	 * \param desc - A description of what the argument is for or
	 * does.
	 * \param req - Whether the argument is required on the command
	 * line.
	 * \param typeDesc - A short, human readable description of the
	 * type that this object expects.  This is used in the generation
	 * of the USAGE statement.  The goal is to be helpful to the end user
	 * of the program.
	 * \param parser - A CmdLine parser object to add this Arg to
	 * \param v - An optional visitor.  You probably should not
	 * use this unless you have a very good reason.
	 */
	MultiArg( const std::string& flag, 
                  const std::string& name,
                  const std::string& desc,
                  bool req,
                  const std::string& typeDesc,
                  CmdLineInterface& parser,
                  Visitor* v = NULL );

	/**
	 * Constructor.
	 * \param flag - The one character flag that identifies this
	 * argument on the command line.
	 * \param name - A one word name for the argument.  Can be
	 * used as a long flag on the command line.
	 * \param desc - A description of what the argument is for or
	 * does.
	 * \param req - Whether the argument is required on the command
	 * line.
	 * \param constraint - A pointer to a Constraint object used
	 * to constrain this Arg.
	 * \param v - An optional visitor.  You probably should not
	 * use this unless you have a very good reason.
	 */
	MultiArg( const std::string& flag,
                  const std::string& name,
                  const std::string& desc,
                  bool req,
                  Constraint<T>* constraint,
                  Visitor* v = NULL );
		  
	/**
	 * Constructor.
	 * \param flag - The one character flag that identifies this
	 * argument on the command line.
	 * \param name - A one word name for the argument.  Can be
	 * used as a long flag on the command line.
	 * \param desc - A description of what the argument is for or
	 * does.
	 * \param req - Whether the argument is required on the command
	 * line.
	 * \param constraint - A pointer to a Constraint object used
	 * to constrain this Arg.
	 * \param parser - A CmdLine parser object to add this Arg to
	 * \param v - An optional visitor.  You probably should not
	 * use this unless you have a very good reason.
	 */
	MultiArg( const std::string& flag, 
                  const std::string& name,
                  const std::string& desc,
                  bool req,
                  Constraint<T>* constraint,
                  CmdLineInterface& parser,
                  Visitor* v = NULL );
		  
	/**
	 * Handles the processing of the argument.
	 * This re-implements the Arg version of this method to set the
	 * _value of the argument appropriately.  It knows the difference
	 * between labeled and unlabeled.
	 * \param i - Pointer the the current argument in the list.
	 * \param args - Mutable list of strings. Passed from main().
	 */
	virtual bool processArg(int* i, std::vector<std::string>& args); 

	/**
	 * Returns a vector of type T containing the values parsed from
	 * the command line.
	 */
	const std::vector<T>& getValue();

	/**
	 * Returns an iterator over the values parsed from the command
	 * line.
	 */
	const_iterator begin() const { return _values.begin(); }

	/**
	 * Returns the end of the values parsed from the command
	 * line.
	 */
	const_iterator end() const { return _values.end(); }

	/**
	 * Returns the a short id string.  Used in the usage. 
	 * \param val - value to be used.
	 */
	virtual std::string shortID(const std::string& val="val") const;

	/**
	 * Returns the a long id string.  Used in the usage. 
	 * \param val - value to be used.
	 */
	virtual std::string longID(const std::string& val="val") const;

	/**
	 * Once we've matched the first value, then the arg is no longer
	 * required.
	 */
	virtual bool isRequired() const;

	virtual bool allowMore();
	
	virtual void reset();

private:
	/**
	 * Prevent accidental copying
	 */
	MultiArg<T>(const MultiArg<T>& rhs);
	MultiArg<T>& operator=(const MultiArg<T>& rhs);

};

template<class T>
MultiArg<T>::MultiArg(const std::string& flag, 
                      const std::string& name,
                      const std::string& desc,
                      bool req,
                      const std::string& typeDesc,
                      Visitor* v) :
   Arg( flag, name, desc, req, true, v ),
  _values(std::vector<T>()),
  _typeDesc( typeDesc ),
  _constraint( NULL ),
  _allowMore(false)
{ 
	_acceptsMultipleValues = true;
}

template<class T>
MultiArg<T>::MultiArg(const std::string& flag, 
                      const std::string& name,
                      const std::string& desc,
                      bool req,
                      const std::string& typeDesc,
                      CmdLineInterface& parser,
                      Visitor* v)
: Arg( flag, name, desc, req, true, v ),
  _values(std::vector<T>()),
  _typeDesc( typeDesc ),
  _constraint( NULL ),
  _allowMore(false)
{ 
	parser.add( this );
	_acceptsMultipleValues = true;
}

/**
 *
 */
template<class T>
MultiArg<T>::MultiArg(const std::string& flag, 
                      const std::string& name,
                      const std::string& desc,
                      bool req,
                      Constraint<T>* constraint,
                      Visitor* v)
: Arg( flag, name, desc, req, true, v ),
  _values(std::vector<T>()),
  _typeDesc( constraint->shortID() ),
  _constraint( constraint ),
  _allowMore(false)
{ 
	_acceptsMultipleValues = true;
}

template<class T>
MultiArg<T>::MultiArg(const std::string& flag, 
                      const std::string& name,
                      const std::string& desc,
                      bool req,
                      Constraint<T>* constraint,
                      CmdLineInterface& parser,
                      Visitor* v)
: Arg( flag, name, desc, req, true, v ),
  _values(std::vector<T>()),
  _typeDesc( constraint->shortID() ),
  _constraint( constraint ),
  _allowMore(false)
{ 
	parser.add( this );
	_acceptsMultipleValues = true;
}

template<class T>
const std::vector<T>& MultiArg<T>::getValue() { return _values; }

template<class T>
bool MultiArg<T>::processArg(int *i, std::vector<std::string>& args) 
{
 	if ( _ignoreable && Arg::ignoreRest() )
		return false;

	if ( _hasBlanks( args[*i] ) )
		return false;

	std::string flag = args[*i];
	std::string value = "";

   	trimFlag( flag, value );

   	if ( argMatches( flag ) )
   	{
   		if ( Arg::delimiter() != ' ' && value == "" )
			throw( ArgParseException( 
			           "Couldn't find delimiter for this argument!",
					   toString() ) );

		// always take the first one, regardless of start string
		if ( value == "" )
		{
			(*i)++;
			if ( static_cast<unsigned int>(*i) < args.size() )
				_extractValue( args[*i] );
			else
				throw( ArgParseException("Missing a value for this argument!",
                                         toString() ) );
		} 
		else
			_extractValue( value );

		/*
		// continuing taking the args until we hit one with a start string 
		while ( (unsigned int)(*i)+1 < args.size() &&
				args[(*i)+1].find_first_of( Arg::flagStartString() ) != 0 &&
		        args[(*i)+1].find_first_of( Arg::nameStartString() ) != 0 ) 
				_extractValue( args[++(*i)] );
		*/

		_alreadySet = true;
		_checkWithVisitor();

		return true;
	}
	else
		return false;
}

/**
 *
 */
template<class T>
std::string MultiArg<T>::shortID(const std::string& val) const
{
	static_cast<void>(val); // Ignore input, don't warn
	return Arg::shortID(_typeDesc) + " ... ";
}

/**
 *
 */
template<class T>
std::string MultiArg<T>::longID(const std::string& val) const
{
	static_cast<void>(val); // Ignore input, don't warn
	return Arg::longID(_typeDesc) + "  (accepted multiple times)";
}

/**
 * Once we've matched the first value, then the arg is no longer
 * required.
 */
template<class T>
bool MultiArg<T>::isRequired() const
{
	if ( _required )
	{
		if ( _values.size() > 1 )
			return false;
		else
			return true;
   	}
   	else
		return false;

}

template<class T>
void MultiArg<T>::_extractValue( const std::string& val ) 
{
    try {
	T tmp;
	ExtractValue(tmp, val, typename ArgTraits<T>::ValueCategory());
	_values.push_back(tmp);
    } catch( ArgParseException &e) {
	throw ArgParseException(e.error(), toString());
    }

    if ( _constraint != NULL )
	if ( ! _constraint->check( _values.back() ) )
	    throw( CmdLineParseException( "Value '" + val +
					  "' does not meet constraint: " +
					  _constraint->description(), 
					  toString() ) );
}
		
template<class T>
bool MultiArg<T>::allowMore()
{
	bool am = _allowMore;
	_allowMore = true;
	return am;
}

template<class T>
void MultiArg<T>::reset()
{
	Arg::reset();
	_values.clear();
}

} // namespace TCLAP

#endif
