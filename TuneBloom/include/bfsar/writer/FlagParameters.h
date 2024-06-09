#pragma once

#include <bfsar/writer/FileWriter.h>

#include <prim/seadBitFlag.h>

class FlagParameters
{
public:
    FlagParameters(const std::unordered_map<u32, u32>& flags)
    {
        for (u32 i = 0; i < 32; i++)
        {
            mFlagEnabled[i] = false;
            mFlagValue[i] = 0;
        }

        for (auto flag : flags)
        {
            mFlagEnabled[flag.first] = true;
            mFlagValue[flag.first] = flag.second;
        }
    }

    void write(FileWriter& writer)
    {
        sead::BitFlag<u32> flags(0);
        for (u32 i = 0; i < 32; i++)
        {
            if (mFlagEnabled[i])
                flags.setBit(i);
        }

        writer.mStream->writeU32(flags.getDirect());

        for (u32 i = 0; i < 32; i++)
        {
            if (mFlagEnabled[i])
                writer.mStream->writeU32(mFlagValue[i]);
        }
    }

    bool mFlagEnabled[32];
    u32 mFlagValue[32];
};
