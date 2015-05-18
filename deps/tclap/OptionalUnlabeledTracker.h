

/****************************************************************************** 
 * 
 *  file:  OptionalUnlabeledTracker.h
 * 
 *  Copyright (c) 2005, Michael E. Smoot .
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


#ifndef TCLAP_OPTIONAL_UNLABELED_TRACKER_H
#define TCLAP_OPTIONAL_UNLABELED_TRACKER_H

#include <string>

namespace TCLAP {

class OptionalUnlabeledTracker
{

	public:

		static void check( bool req, const std::string& argName );

		static void gotOptional() { alreadyOptionalRef() = true; }

		static bool& alreadyOptional() { return alreadyOptionalRef(); } 

	private:

		static bool& alreadyOptionalRef() { static bool ct = false; return ct; }
};


inline void OptionalUnlabeledTracker::check( bool req, const std::string& argName )
{
    if ( OptionalUnlabeledTracker::alreadyOptional() )
        throw( SpecificationException(
	"You can't specify ANY Unlabeled Arg following an optional Unlabeled Arg",
	                argName ) );

    if ( !req )
        OptionalUnlabeledTracker::gotOptional();
}


} // namespace TCLAP

#endif
