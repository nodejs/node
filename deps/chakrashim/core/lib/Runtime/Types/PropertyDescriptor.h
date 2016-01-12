//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct PropertyDescriptor
    {
    public:
        PropertyDescriptor();

    private:
        Var Value;
        Var Getter;
        Var Setter;
        Var originalVar;

        bool writableSpecified;
        bool enumerableSpecified;
        bool configurableSpecified;
        bool valueSpecified;
        bool getterSpecified;
        bool setterSpecified;

        bool Writable;
        bool Enumerable;
        bool Configurable;
        bool fromProxy;

    public:
        bool IsDataDescriptor() const { return writableSpecified | valueSpecified;}
        bool IsAccessorDescriptor() const { return getterSpecified | setterSpecified;}
        bool IsGenericDescriptor() const { return !IsAccessorDescriptor() && !IsDataDescriptor(); }
        void SetEnumerable(bool value);
        void SetWritable(bool value);
        void SetConfigurable(bool value);

        void SetValue(Var value);
        Var GetValue() const { return Value; }

        void SetGetter(Var getter);
        Var GetGetter() const { Assert(getterSpecified || Getter == nullptr);  return Getter; }
        void SetSetter(Var setter);
        Var GetSetter() const { Assert(setterSpecified || Setter == nullptr); return Setter; }

        PropertyAttributes GetAttributes() const;

        bool IsFromProxy() const { return fromProxy; }
        void SetFromProxy(bool value) { fromProxy = value; }

        void SetOriginal(Var orginal) { originalVar = orginal; }
        Var GetOriginal() const { return originalVar; }

        bool ValueSpecified() const { return valueSpecified; }
        bool WritableSpecified() const { return writableSpecified; };
        bool ConfigurableSpecified() const { return configurableSpecified; }
        bool EnumerableSpecified() const { return enumerableSpecified; }
        bool GetterSpecified() const { return getterSpecified; }
        bool SetterSpecified() const { return setterSpecified; }

        bool IsWritable() const { Assert(writableSpecified);  return Writable; }
        bool IsEnumerable() const { Assert(enumerableSpecified); return Enumerable; }
        bool IsConfigurable() const { Assert(configurableSpecified);  return Configurable; }

        // Set configurable/enumerable/writable.
        // attributes: attribute values.
        // mask: specified which attributes to set. If an attribute is not in the mask.
        void SetAttributes(PropertyAttributes attributes, PropertyAttributes mask = ~PropertyNone);

        // Merge from descriptor parameter into this but only fields specified by descriptor parameter.
        void MergeFrom(const PropertyDescriptor& descriptor);
    };
}
