//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// -----------------------------------------------------------------------------------------------------------------------------
// Template parameter constraints
// See http://www.stroustrup.com/bs_faq2.html#constraints
// -----------------------------------------------------------------------------------------------------------------------------

namespace TemplateParameter
{
    template<class T, class Base>
    class SameOrDerivedFrom
    {
    private:
        static void Constrain(T *const t)
        {
#pragma warning(suppress: 4189) // C4189: local variable is initialized but not referenced
            Base *const b = t;
        }

    public:
        SameOrDerivedFrom()
        {
#pragma warning(suppress: 4189) // C4189: local variable is initialized but not referenced
            void (*const p)(T *const t) = Constrain;
        }
    };

    template<class T>
    struct Box
    {
    };
};
