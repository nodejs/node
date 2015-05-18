
/****************************************************************************** 
 * 
 *  file:  IgnoreRestVisitor.h
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


#ifndef TCLAP_IGNORE_REST_VISITOR_H
#define TCLAP_IGNORE_REST_VISITOR_H

#include "Visitor.h"
#include "Arg.h"

namespace TCLAP {

/**
 * A Vistor that tells the CmdLine to begin ignoring arguments after
 * this one is parsed.
 */
class IgnoreRestVisitor: public Visitor
{
	public:

		/**
		 * Constructor.
		 */
		IgnoreRestVisitor() : Visitor() {}

		/**
		 * Sets Arg::_ignoreRest.
		 */
		void visit() { Arg::beginIgnoring();  }
};

}

#endif
