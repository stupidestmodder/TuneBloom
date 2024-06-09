#pragma once

#include <snd/ut/ut_BinaryFileFormat.h>
#include <snd/snd_Global.h>
#include <snd/snd_ItemId.h>

#include <basis/seadAssert.h>
#include <prim/seadPtrUtil.h>

namespace nw { namespace snd { namespace internal {

class Util
{
public:
    static u32 GetSampleByByte(u32 byte, SampleFormat format)
    {
        u32 samples = 0;
        u32 frac;

        switch (format)
        {
            case SAMPLE_FORMAT_DSP_ADPCM:
                samples = (byte / 8) * 14;
                frac = (byte % 8);

                if (frac != 0)
                    samples += (frac - 1) * 2;

                break;

            case SAMPLE_FORMAT_PCM_S8:
                samples = byte;
                break;

            case SAMPLE_FORMAT_PCM_S16:
                samples = (byte >> 1);
                break;

            default:
                SEAD_ASSERT_MSG(false, "Invalid format\n");
                break;
        }

        return samples;
    }

    static u32 GetByteBySample(u32 samples, SampleFormat format)
    {
        u32 byte = 0;
        u32 frac;

        switch (format)
        {
            case SAMPLE_FORMAT_DSP_ADPCM:
                byte = (samples / 14) * 8;
                frac = (samples % 14);

                if (frac != 0)
                    byte += ((frac + 1) >> 1) + 1;

                break;

            case SAMPLE_FORMAT_PCM_S8:
                byte = samples;
                break;

            case SAMPLE_FORMAT_PCM_S16:
                byte = (samples << 1);
                break;

            default:
                SEAD_ASSERT_MSG(false, "Invalid format\n");
                break;
        }

        return byte;
    }

    template <typename ITEM_TYPE, typename COUNT_TYPE = ut::ResU32>
    struct Table
    {
        COUNT_TYPE count;
        ITEM_TYPE item[1];
    };
    // Can't check size

    struct Reference
    {
        ut::ResU16 typeId;
        u16 padding;
        ut::ResS32 offset;

        static const s32 INVALID_OFFSET = -1;

        bool IsValidTypeId(u16 validId) const
        {
            return validId == typeId;
        }

        bool IsValidOffset() const
        {
            return offset != INVALID_OFFSET;
        }
    };
    static_assert(sizeof(Reference) == 0x8);

    struct ReferenceWithSize : public Reference
    {
        ut::ResU32 size;
    };
    static_assert(sizeof(ReferenceWithSize) == 0xC);

    struct ReferenceTable : public Table<Reference>
    {
        const void* GetReferedItem(u32 index) const
        {
            if (index >= count)
                return nullptr;

            return sead::PtrUtil::addOffset(this, item[index].offset);
        }

        const void* GetReferedItem(u32 index, u16 typeId) const
        {
            if (index >= count)
                return nullptr;

            if (item[index].typeId != typeId)
                return nullptr;

            return sead::PtrUtil::addOffset(this, item[index].offset);
        }

        const void* FindReferedItemBy(u16 typeId) const
        {
            for (u32 i = 0; i < count; i++)
            {
                if (item[i].IsValidTypeId(typeId))
                {
                    return sead::PtrUtil::addOffset(this, item[i].offset);
                }
            }

            return nullptr;
        }
    };
    // Can't check size

    struct ReferenceWithSizeTable : public Table<ReferenceWithSize>
    {
        const void* GetReferedItem(u32 index) const
        {
            SEAD_ASSERT(index < count);
            return sead::PtrUtil::addOffset(this, item[index].offset);
        }

        const void* GetReferedItemBy(u16 typeId) const
        {
            for (u32 i = 0; i < count; i++)
            {
                if (item[i].IsValidTypeId(typeId))
                    return sead::PtrUtil::addOffset(this, item[i].offset);
            }

            return nullptr;
        }

        u32 GetReferedItemSize(u32 index) const
        {
            SEAD_ASSERT(index < count);
            return item[index].size;
        }
    };
    // Can't check size

    struct BlockReferenceTable
    {
        ReferenceWithSize item[1];

        const void* GetReferedItemByIndex(const void* origin, int index, u16 count) const
        {
            SEAD_ASSERT(index < count);
            return sead::PtrUtil::addOffset(origin, item[index].offset);
        }

        const ReferenceWithSize* GetReference(u16 typeId, u16 count) const
        {
            for (s32 i = 0; i < count; i++)
            {
                if (item[i].IsValidTypeId(typeId))
                    return &item[i];
            }

            return nullptr;
        }

        const void* GetReferedItem(const void* origin, u16 typeId, u16 count) const
        {
            const ReferenceWithSize* ref = GetReference(typeId, count);
            if (!ref)
                return nullptr;

            if (ref->offset == 0)
                return nullptr;

            return sead::PtrUtil::addOffset(origin, ref->offset);
        }

        u32 GetReferedItemSize(u16 typeId, u16 count) const
        {
            const ReferenceWithSize* ref = GetReference(typeId, count);
            if (!ref)
                return 0;

            return ref->size;
        }

        u32 GetReferedItemOffset(u16 typeId, u16 count) const
        {
            const ReferenceWithSize* ref = GetReference(typeId, count);
            if (!ref)
                return 0;

            return ref->offset;
        }
    };
    // Can't check size

    struct SoundFileHeader
    {
        ut::BinaryFileHeader header;
        BlockReferenceTable blockReferenceTable;

        s32 GetBlockCount() const { return header.dataBlocks; }

        const void* GetBlock(u16 typeId) const
        {
            return blockReferenceTable.GetReferedItem(this, typeId, header.dataBlocks);
        }

        u32 GetBlockSize(u16 typeId) const
        {
            return blockReferenceTable.GetReferedItemSize(typeId, header.dataBlocks);
        }

        u32 GetBlockOffset(u16 typeId) const
        {
            return blockReferenceTable.GetReferedItemOffset(typeId, header.dataBlocks);
        }
    };
    // Can't check size

    struct BitFlag
    {
        ut::ResU32 bitFlag;

        bool GetValue(u32* value, u32 bitNumber) const
        {
            u32 count = GetTrueCount(bitNumber);
            if (count == 0)
                return false;

            *value = *reinterpret_cast<const ut::ResU32*>(sead::PtrUtil::addOffset(this, count * sizeof(ut::ResU32)));
            return true;
        }

        bool GetValueF32(f32* value, u32 bitNumber) const
        {
            u32 count = GetTrueCount(bitNumber);
            if (count == 0)
                return false;

            *value = *reinterpret_cast<const ut::ResF32*>(sead::PtrUtil::addOffset(this, count * sizeof(ut::ResF32)));
            return true;
        }

        static const s32 BIT_NUMBER_MAX = 31;
        u32 GetTrueCount(u32 bitNumber) const
        {
            SEAD_ASSERT(bitNumber <= BIT_NUMBER_MAX);

            bool ret = false;
            s32 count = 0;
            for (u32 i = 0; i <= bitNumber; i++)
            {
                if (bitFlag & (0x1 << i))
                {
                    count++;
                    if (i == bitNumber)
                        ret = true;
                }
            }

            if (ret)
                return count;
            else
                return 0;
        }
    };
    static_assert(sizeof(BitFlag) == 0x4);

    static u8 DevideBy8bit(u32 value, s32 index)
    {
        return static_cast<u8>((value >> (8 * index)) & 0xFF);
    }

    static u16 DevideBy16bit(u32 value, s32 index)
    {
        return static_cast<u16>((value >> (16 * index)) & 0xFFFF);
    }

    static ItemType GetItemType(u32 id)
    {
        return static_cast<ItemType>(id >> 24);
    }

    static u32 GetItemIndex(u32 id)
    {
        return id & 0x00FFFFFF;
    }

    static u32 GetMaskedItemId(u32 id, internal::ItemType type)
    {
        return id | (type << 24);
    }

    struct WaveId
    {
        ut::ResU32 waveArchiveId;
        ut::ResU32 waveIndex;
    };

    struct WaveIdTable
    {
        Table<WaveId> table;

        const WaveId* GetWaveId(u32 index) const
        {
            if (index >= table.count)
                return nullptr;

            return &table.item[index];
        }
        u32 GetCount() const { return table.count; }
    };
};

} } } // namespace nw::snd::internal
