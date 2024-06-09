#pragma once

#include <snd/ut/res/ut_ResTypes.h>

#include <prim/seadPtrUtil.h>

namespace nw { namespace ut {

struct BinaryFileHeader
{
    char signature[4];
    ResU16 byteOrder;
    ResU16 headerSize;
    ResU32 version;
    ResU32 fileSize;
    ResU16 dataBlocks;
    ResU16 reserved;
};
static_assert(sizeof(BinaryFileHeader) == 0x14);

struct BinaryBlockHeader
{
    char kind[4];
    ResU32 size;
};
static_assert(sizeof(BinaryBlockHeader) == 0x8);

inline sead::Endian::Types GetFileEndian(const BinaryFileHeader& header)
{
    const void* byteOrder = sead::PtrUtil::addOffset(&header, offsetof(BinaryFileHeader, byteOrder));

    return sead::Endian::markToEndian(*(u16*)byteOrder);
}

} } // namespace nw::ut
