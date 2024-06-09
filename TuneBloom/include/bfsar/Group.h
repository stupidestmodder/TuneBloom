#pragma once

#include <bfsar/Item.h>

class Group : public Item
{
public:
    enum class OutputType
    {
        Embed,
        Link,
        // External
    };

public:
    Group()
        : Item()
        , mOutputType(OutputType::Embed)
    {
        mItemType = ItemType::Group;
    }

    OutputType getOutputType() const
    {
        return mOutputType;
    }

    void setOutputType(OutputType outputType)
    {
        mOutputType = outputType;
    }

private:
    OutputType mOutputType;

    friend class Bfsar;
};
