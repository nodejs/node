
/****************************************************************************** 
 * 
 *  file:  UnlabeledMultiArg.h
 * 
 *  Copyright (c) 2003, Michael E. Smoot.
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


#ifndef TCLAP_MULTIPLE_UNLABELED_ARGUMENT_H
#define TCLAP_MULTIPLE_UNLABELED_ARGUMENT_H

#include <string>
#include <vector>

#include "MultiArg.h"
#include "OptionalUnlabeledTracker.h"

namespace TCLAP {

/**
 * Just like a MultiArg, except that the arguments are unlabeled.  Basically,
 * this Arg will slurp up everything that hasn't been matched to another 
 * Arg.
 */
template<class T>
class UnlabeledMultiArg : public MultiArg<T>
{

	// If compiler has two stage name lookup (as gcc >= 3.4 does)
	// this is requried to prevent undef. symbols
	using MultiArg<T>::_ignoreable;
	using MultiArg<T>::_hasBlanks;
	using MultiArg<T>::_extractValue;
	using MultiArg<T>::_typeDesc;
	using MultiArg<T>::_name;
	using MultiArg<T>::_description;
	using MultiArg<T>::_alreadySet;
	using MultiArg<T>::toString;

	public:
		
		/**
		 * Constructor.  
		 * \param name - The name of the Arg. Note that this is used for
		 * identification, not as a long flag.
		 * \param desc - A description of what the argument is for or
		 * does.
		 * \param req - Whether the argument is required on the command
		 *  line.
		 * \param typeDesc - A short, human readable description of the
		 * type that this object expects.  This is used in the generation
		 * of the USAGE statement.  The goal is to be helpful to the end user
		 * of the program.
		 * \param ignoreable - Whether or not this argument can be ignored
		 * using the "--" flag.
		 * \param v - An optional visitor.  You probably should not
		 * use this unless you have a very good reason.
		 */
		UnlabeledMultiArg( const std::string& name,
				           const std::string& desc,
						   bool req,
				           const std::string& typeDesc,
						   bool ignoreable = false,
				           Visitor* v = NULL );
		/**
		 * Constructor.  
		 * \param name - The name of the Arg. Note that this is used for
		 * identification, not as a long flag.
		 * \param desc - A description of what the argument is for or
		 * does.
		 * \param req - Whether the argument is required on the command
		 *  line.
		 * \param typeDesc - A short, human readable description of the
		 * type that this object expects.  This is used in the generation
		 * of the USAGE statement.  The goal is to be helpful to the end user
		 * of the program.
		 * \param parser - A CmdLine parser object to add this Arg to
		 * \param ignoreable - Whether or not this argument can be ignored
		 * using the "--" flag.
		 * \param v - An optional visitor.  You probably should not
		 * use this unless you have a very good reason.
		 */
		UnlabeledMultiArg( const std::string& name,
				           const std::string& desc,
						   bool req,
				           const std::string& typeDesc,
						   CmdLineInterface& parser,
						   bool ignoreable = false,
				           Visitor* v = NULL );
						 
		/**
		 * Constructor.  
		 * \param name - The name of the Arg. Note that this is used for
		 * identification, not as a long flag.
		 * \param desc - A description of what the argument is for or
		 * does.
		 * \param req - Whether the argument is required on the command
		 *  line.
		 * \param constraint - A pointer to a Constraint object used
		 * to constrain this Arg.
		 * \param ignoreable - Whether or not this argument can be ignored
		 * using the "--" flag.
		 * \param v - An optional visitor.  You probably should not
		 * use this unless you have a very good reason.
		 */
		UnlabeledMultiArg( const std::string& name,
						   const std::string& desc,
						   bool req,
						   Constraint<T>* constraint,
						   bool ignoreable = false,
						   Visitor* v = NULL );

		/**
		 * Constructor.  
		 * \param name - The name of the Arg. Note that this is used for
		 * identification, not as a long flag.
		 * \param desc - A description of what the argument is for or
		 * does.
		 * \param req - Whether the argument is required on the command
		 *  line.
		 * \param constraint - A pointer to a Constraint object used
		 * to constrain this Arg.
		 * \param parser - A CmdLine parser object to add this Arg to
		 * \param ignoreable - Whether or not this argument can be ignored
		 * using the "--" flag.
		 * \param v - An optional visitor.  You probably should not
		 * use this unless you have a very good reason.
		 */
		UnlabeledMultiArg( const std::string& name, 
						   const std::string& desc, 
						   bool req,
						   Constraint<T>* constraint,
						   CmdLineInterface& parser,
						   bool ignoreable = false,
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
		 * Opertor ==.
		 * \param a - The Arg to be compared to this.
		 */
		virtual bool operator==(const Arg& a) const;

		/**
		 * Pushes this to back of list rather than front.
		 * \param argList - The list this should be added to.
		 */
		virtual void addToList( std::list<Arg*>& argList ) const;
};

template<class T>
UnlabeledMultiArg<T>::UnlabeledMultiArg(const std::string& name, 
				                        const std::string& desc, 
										bool req,
					                    const std::string& typeDesc,
										bool ignoreable,
					                    Visitor* v)
: MultiArg<T>("", name, desc,  req, typeDesc, v)
{ 
	_ignoreable = ignoreable;
	OptionalUnlabeledTracker::check(true, toString());
}

template<class T>
UnlabeledMultiArg<T>::UnlabeledMultiArg(const std::string& name, 
				                        const std::string& desc, 
										bool req,
					                    const std::string& typeDesc,
										CmdLineInterface& parser,
										bool ignoreable,
					                    Visitor* v)
: MultiArg<T>("", name, desc,  req, typeDesc, v)
{ 
	_ignoreable = ignoreable;
	OptionalUnlabeledTracker::check(true, toString());
	parser.add( this );
}


template<class T>
UnlabeledMultiArg<T>::UnlabeledMultiArg(const std::string& name, 
				                        const std::string& desc, 
										bool req,
					                    Constraint<T>* constraint,
										bool ignoreable,
					                    Visitor* v)
: MultiArg<T>("", name, desc,  req, constraint, v)
{ 
	_ignoreable = ignoreable;
	OptionalUnlabeledTracker::check(true, toString());
}

template<class T>
UnlabeledMultiArg<T>::UnlabeledMultiArg(const std::string& name, 
				                        const std::string& desc, 
										bool req,
					                    Constraint<T>* constraint,
										CmdLineInterface& parser,
										bool ignoreable,
					                    Visitor* v)
: MultiArg<T>("", name, desc,  req, constraint, v)
{ 
	_ignoreable = ignoreable;
	OptionalUnlabeledTracker::check(true, toString());
	parser.add( this );
}


template<class T>
bool UnlabeledMultiArg<T>::processArg(int *i, std::vector<std::string>& args) 
{

	if ( _hasBlanks( args[*i] ) )
		return false;

	// never ignore an unlabeled multi arg


	// always take the first value, regardless of the start string 
	_extractValue( args[(*i)] );

	/*
	// continue taking args until we hit the end or a start string 
	while ( (unsigned int)(*i)+1 < args.size() &&
			args[(*i)+1].find_first_of( Arg::flagStartString() ) != 0 &&
            args[(*i)+1].find_first_of( Arg::nameStartString() ) != 0 ) 
		_extractValue( args[++(*i)] );
	*/

	_alreadySet = true;

	return true;
}

template<class T>
std::string UnlabeledMultiArg<T>::shortID(const std::string& val) const
{
	static_cast<void>(val); // Ignore input, don't warn
	return std::string("<") + _typeDesc + "> ...";
}

template<class T>
std::string UnlabeledMultiArg<T>::longID(const std::string& val) const
{
	static_cast<void>(val); // Ignore input, don't warn
	return std::string("<") + _typeDesc + ">  (accepted multiple times)";
}

template<class T>
bool UnlabeledMultiArg<T>::operator==(const Arg& a) const
{
	if ( _name == a.getName() || _description == a.getDescription() )
		return true;
	else
		return false;
}

template<class T>
void UnlabeledMultiArg<T>::addToList( std::list<Arg*>& argList ) const
{
	argList.push_back( const_cast<Arg*>(static_cast<const Arg* const>(this)) );
}

}

#endif
