#pragma once

#include <prim/seadRuntimeTypeInfo.h>

#include <snd/MmlParser.h>

#include <format>
#include <string>
#include <vector>

class SeqArgBase
{
    SEAD_RTTI_BASE(SeqArgBase);

public:
    SeqArgBase()
    {
    }

    virtual ~SeqArgBase()
    {
    }

    virtual void read(const u8*& ptr) = 0;
    virtual std::vector<u8> encode() const = 0;
    virtual std::string toString() const = 0;
};

class SeqArg8 : public SeqArgBase
{
    SEAD_RTTI_OVERRIDE(SeqArg8, SeqArgBase);

public:
    SeqArg8(const u8*& ptr, bool hasSign = false)
        : mHasSign(hasSign)
    {
        read(ptr);
    }

    SeqArg8(u8 value, bool hasSign = false)
        : mHasSign(hasSign)
    {
        set(value);
    }

    void read(const u8*& ptr) override
    {
        set(MmlParser::ReadByte(&ptr));
    }

    void set(u8 value)
    {
        mValue = value;
    }

    std::vector<u8> encode() const override
    {
        return { mValue };
    }

    std::string toString() const override
    {
        if (mHasSign)
            return std::format("{:d}", (s32)(s8)mValue);

        return std::format("{:d}", (u32)mValue);
    }

    bool mHasSign;
    u8 mValue;
};

class SeqArg16 : public SeqArgBase
{
    SEAD_RTTI_OVERRIDE(SeqArg16, SeqArgBase);

public:
    SeqArg16(const u8*& ptr, bool hasSign = false)
        : mHasSign(hasSign)
    {
        read(ptr);
    }

    SeqArg16(u16 value, bool hasSign = false)
        : mHasSign(hasSign)
    {
        set(value);
    }

    void read(const u8*& ptr) override
    {
        set(MmlParser::Read16(&ptr));
    }

    void set(u16 value)
    {
        mValue = value;
    }

    std::vector<u8> encode() const override
    {
        return { (u8)(mValue >> 8), (u8)mValue };
    }

    std::string toString() const override
    {
        if (mHasSign)
            return std::format("{:d}", (s32)(s16)mValue);

        return std::format("{:d}", (u32)mValue);
    }

    bool mHasSign;
    u16 mValue;
};

class SeqArgVMIDI : public SeqArgBase
{
    SEAD_RTTI_OVERRIDE(SeqArgVMIDI, SeqArgBase);

public:
    SeqArgVMIDI(const u8*& ptr)
    {
        read(ptr);
    }

    SeqArgVMIDI(u16 value)
    {
        set(value);
    }

    void read(const u8*& ptr) override
    {
        set(MmlParser::ReadVar(&ptr));
    }

    void set(u32 value)
    {
        SEAD_ASSERT(value <= 0x0FFFFFFF);
        mValue = value;
    }

    std::vector<u8> encode() const override
    {
        if (mValue == 0)
            return { 0 };

        u32 runs = (std::bit_width(mValue) + 6) / 7;
        SEAD_ASSERT(1 <= runs <= 4);
        std::vector<u8> ret;

        for (u32 i = runs - 1; i > 0; i--)
        {
            ret.push_back(((mValue >> (i * 7)) & 0b01111111) | 0b10000000);
        }

        ret.push_back(mValue & 0b01111111);
        return ret;
    }

    std::string toString() const override
    {
        return std::format("{:d}", mValue);
    }

    u32 mValue;
};

class SeqArgVariable : public SeqArgBase
{
    SEAD_RTTI_OVERRIDE(SeqArgVariable, SeqArgBase);

public:
    SeqArgVariable(const u8*& ptr)
    {
        read(ptr);
    }

    SeqArgVariable(u8 value)
    {
        set(value);
    }

    void read(const u8*& ptr) override
    {
        set(MmlParser::ReadByte(&ptr));
    }

    void set(u8 varNo)
    {
        SEAD_ASSERT(0 <= varNo && varNo < 16 * 3);
        mValue = varNo;
    }

    std::vector<u8> encode() const override
    {
        SEAD_ASSERT(0 <= mValue && mValue < 16 * 3);
        return { mValue };
    }

    std::string toString() const override
    {
        SEAD_ASSERT(0 <= mValue && mValue < 16 * 3);

        switch (mValue / 16)
        {
            case 0:
                return std::format("VAR_{:d}", (u32)mValue % 16);
            case 1:
                return std::format("GVAR_{:d}", (u32)mValue % 16);
            case 2:
                return std::format("TVAR_{:d}", (u32)mValue % 16);
        }

        return "(Invalid)";
    }

    u8 mValue;
};

class SeqArgRandom : public SeqArgBase
{
    SEAD_RTTI_OVERRIDE(SeqArgRandom, SeqArgBase);

public:
    SeqArgRandom(const u8*& ptr)
    {
        read(ptr);
    }

    SeqArgRandom(s16 min, s16 max)
    {
        set(min, max);
    }

    void read(const u8*& ptr) override
    {
        s16 min = (s16)MmlParser::Read16(&ptr);
        s16 max = (s16)MmlParser::Read16(&ptr);
        set(min, max);
    }

    void set(s16 min, s16 max)
    {
        mMin = min;
        mMax = max;
    }

    std::vector<u8> encode() const override
    {
        return { (u8)(mMin >> 8), (u8)mMin, (u8)(mMax >> 8), (u8)mMax };
    }

    std::string toString() const override
    {
        return std::format("{:d}, {:d}", (s32)mMin, (s32)mMax);
    }

    s16 mMin;
    s16 mMax;
};
