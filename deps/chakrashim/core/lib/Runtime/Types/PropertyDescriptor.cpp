//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    PropertyDescriptor::PropertyDescriptor() :
        writableSpecified(false),
        enumerableSpecified(false),
        configurableSpecified(false),
        valueSpecified(false),
        getterSpecified(false),
        setterSpecified(false),
        Writable(false),
        Enumerable(false),
        Configurable(false),
        Value(nullptr),
        Getter(nullptr),
        Setter(nullptr),
        originalVar(nullptr),
        fromProxy(false)
    {
    }

    void PropertyDescriptor::SetEnumerable(bool value)
    {
        Enumerable = value;
        enumerableSpecified = true;
    }

    void PropertyDescriptor::SetWritable(bool value)
    {
        Writable = value;
        writableSpecified = true;
    }

    void PropertyDescriptor::SetConfigurable(bool value)
    {
        Configurable = value;
        configurableSpecified = true;
    }

    void PropertyDescriptor::SetValue(Var value)
    {
        this->Value = value;
        this->valueSpecified = true;
    }

    void PropertyDescriptor::SetGetter(Var getter)
    {
        this->Getter = getter;
        this->getterSpecified = true;
    }

    void PropertyDescriptor::SetSetter(Var setter)
    {
        this->Setter = setter;
        this->setterSpecified = true;
    }

    PropertyAttributes PropertyDescriptor::GetAttributes() const
    {
        PropertyAttributes attributes = PropertyNone;

        if (this->configurableSpecified && this->Configurable)
        {
            attributes |= PropertyConfigurable;
        }
        if (this->enumerableSpecified && this->Enumerable)
        {
            attributes |= PropertyEnumerable;
        }
        if (this->writableSpecified && this->Writable)
        {
            attributes |= PropertyWritable;
        }

        return attributes;
    }

    void PropertyDescriptor::SetAttributes(PropertyAttributes attributes, PropertyAttributes mask)
    {
        if (mask & PropertyConfigurable)
        {
            this->SetConfigurable(PropertyNone != (attributes & PropertyConfigurable));
        }
        if (mask & PropertyEnumerable)
        {
            this->SetEnumerable(PropertyNone != (attributes & PropertyEnumerable));
        }
        if (mask & PropertyWritable)
        {
            this->SetWritable(PropertyNone != (attributes & PropertyWritable));
        }
    }

    void PropertyDescriptor::MergeFrom(const PropertyDescriptor& descriptor)
    {
        if (descriptor.configurableSpecified)
        {
            this->SetConfigurable(descriptor.Configurable);
        }
        if (descriptor.enumerableSpecified)
        {
            this->SetEnumerable(descriptor.Enumerable);
        }
        if (descriptor.writableSpecified)
        {
            this->SetWritable(descriptor.Writable);
        }

        if (descriptor.valueSpecified)
        {
            this->SetValue(descriptor.Value);
        }
        if (descriptor.getterSpecified)
        {
            this->SetGetter(descriptor.Getter);
        }
        if (descriptor.setterSpecified)
        {
            this->SetSetter(descriptor.Setter);
        }
    }
}
