#pragma once

#include <snd/ut/ut_BinaryFileFormat.h>
#include <snd/snd_ElementType.h>
#include <snd/snd_Global.h>
#include <snd/snd_ItemId.h>
#include <snd/snd_SoundArchive.h>
#include <snd/snd_StreamSoundFile.h>
#include <snd/snd_Util.h>

#include <basis/seadAssert.h>
#include <prim/seadPtrUtil.h>
#include <prim/seadSafeString.h>

namespace nw { namespace snd { namespace internal {

const u32 DEFAULT_STRING_ID = SoundArchive::INVALID_ID;
const PanMode DEFAULT_PAN_MODE = PAN_MODE_DUAL;
const PanCurve DEFAULT_PAN_CURVE = PAN_CURVE_SQRT;
const u8 DEFAULT_PLAYER_PRIORITY = 64;
const u8 DEFAULT_CHANNEL_PRIORITY = 64;
const u8 DEFAULT_ACTOR_PLAYER_ID = 0;
const u8 DEFAULT_IS_RELEASE_PRIORITY_FIX = 0;
const bool DEFAULT_IS_FRONT_BYPASS = false;
const u32 DEFAULT_USER_PARAM = 0xFFFFFFFF;
const u32 DEFAULT_SEQ_START_OFFSET = 0;
const u32 DEFAULT_WARC_WAVE_COUNT = 0;
const u32 DEFAULT_PLAYER_HEAP_SIZE = 0;

const s32 USER_PARAM_COUNT = 4;

enum SoundInfoBitFlag
{
    SOUND_INFO_STRING_ID = 0x00,
    SOUND_INFO_PAN_PARAM,
    SOUND_INFO_PLAYER_PARAM,

    SOUND_INFO_OFFSET_TO_3D_PARAM = 0x08,
    //SOUND_INFO_OFFSET_TO_SEND_PARAM,
    //SOUND_INFO_OFFSET_TO_MOD_PARAM,

    //SOUND_INFO_OFFSET_TO_RVL_PARAM = 0x10,
    SOUND_INFO_OFFSET_TO_CTR_PARAM = 0x11,
    //SOUND_INFO_OFFSET_TO_CAFE_PARAM,

    SOUND_INFO_USER_PARAM3 = 0x1C,
    SOUND_INFO_USER_PARAM2 = 0x1D,
    SOUND_INFO_USER_PARAM1 = 0x1E,
    SOUND_INFO_USER_PARAM = 0x1F
};

const s32 USER_PARAM_INDEX[USER_PARAM_COUNT] = {
    SOUND_INFO_USER_PARAM,
    SOUND_INFO_USER_PARAM1,
    SOUND_INFO_USER_PARAM2,
    SOUND_INFO_USER_PARAM3
};

enum WaveSoundInfoBitFlag
{
    WAVE_SOUND_INFO_PRIORITY = 0x00
};

enum SequenceSoundInfoBitFlag
{
    SEQ_SOUND_INFO_START_OFFSET = 0x00,
    SEQ_SOUND_INFO_PRIORITY
};

enum BankInfoBitFlag
{
    BANK_INFO_STRING_ID = 0x00
};

enum PlayerInfoBitFlag
{
    PLAYER_INFO_STRING_ID = 0x00,
    PLAYER_INFO_HEAP_SIZE
};

enum SoundGroupInfoBitFlag
{
    SOUND_GROUP_INFO_STRING_ID = 0x00
};

enum GroupInfoBitFlag
{
    GROUP_INFO_STRING_ID = 0x00
};

enum WaveArchiveInfoBitFlag
{
    WAVE_ARCHIVE_INFO_STRING_ID = 0x00,
    WAVE_ARCHIVE_INFO_WAVE_COUNT
};

inline ItemType GetItemTypeEnum(u32 id)
{
    return static_cast<ItemType>(Util::GetItemType(id));
}

class SoundArchiveFile
{
public:
    static const s32 BLOCK_SIZE = 3;

    struct FileHeader : public ut::BinaryFileHeader
    {
        Util::ReferenceWithSize toBlocks[BLOCK_SIZE];

        u32 GetStringBlockSize() const
        {
            return GetReferenceBy(ElementType_SoundArchiveFile_StringBlock)->size;
        }
        u32 GetInfoBlockSize() const
        {
            return GetReferenceBy(ElementType_SoundArchiveFile_InfoBlock)->size;
        }
        u32 GetFileBlockSize() const
        {
            return GetReferenceBy(ElementType_SoundArchiveFile_FileBlock)->size;
        }

        s32 GetStringBlockOffset() const
        {
            return GetReferenceBy(ElementType_SoundArchiveFile_StringBlock)->offset;
        }
        s32 GetInfoBlockOffset() const
        {
            return GetReferenceBy(ElementType_SoundArchiveFile_InfoBlock)->offset;
        }
        s32 GetFileBlockOffset() const
        {
            return GetReferenceBy(ElementType_SoundArchiveFile_FileBlock)->offset;
        }

        const Util::ReferenceWithSize* GetReferenceBy(u16 typeId) const
        {
            for (s32 i = 0; i < BLOCK_SIZE; i++)
            {
                if (toBlocks[i].typeId == typeId)
                    return &toBlocks[i];
            }

            SEAD_ASSERT(false);
            return nullptr;
        }
    };
    static_assert(sizeof(FileHeader) == 0x38);

    struct StringTable
    {
        Util::ReferenceWithSizeTable table;

        const char* GetString(s32 stringId) const
        {
            return reinterpret_cast<const char*>(sead::PtrUtil::addOffset(this, table.item[stringId].offset));
        }

        u32 GetCount() const
        {
            return table.count;
        }
    };
    // Can't check size

    struct PatriciaTree
    {
        struct NodeData
        {
            ut::ResU32 stringId;
            ut::ResU32 itemId;
        };
        static_assert(sizeof(NodeData) == 0x8);

        struct Node
        {
            static const u16 FLAG_LEAF = 1 << 0;

            ut::ResU16 flags;
            ut::ResU16 bit;
            ut::ResU32 leftIdx;
            ut::ResU32 rightIdx;
            NodeData nodeData;
        };
        static_assert(sizeof(Node) == 0x14);

        ut::ResU32 rootIdx;
        Util::Table<Node> nodeTable;

        const NodeData* GetNodeDataBy(const char* str, size_t len = 0) const
        {
            if (rootIdx >= nodeTable.count)
                return nullptr;

            const Node* node = &nodeTable.item[rootIdx];
            if (len == 0)
                len = sead::SafeString(str).calcLength();

            while ((node->flags & Node::FLAG_LEAF) == 0)
            {
                const s32 pos = node->bit >> 3;
                const s32 bit = node->bit & 7;

                u32 nodeIdx;
                if (pos < static_cast<s32>(len) && str[pos] & (1 << (7 - bit)))
                    nodeIdx = node->rightIdx;
                else
                    nodeIdx = node->leftIdx;

                node = &nodeTable.item[nodeIdx];
            }

            return &node->nodeData;
        }

        u32 GetNumData() const { return nodeTable.count; }
        u32 GetCount() const { return GetNumData(); }
    };
    // Can't check size

    struct StringBlockBody
    {
        enum Sections
        {
            Sections_StringTable,
            Sections_PatriciaTree,
            Sections_Max = Sections_PatriciaTree
        };

        Util::Reference toSection[Sections_Max + 1];

        const char* GetString(SoundArchive::ItemId stringId) const
        {
            if (stringId == SoundArchive::INVALID_ID)
                return nullptr;

            const StringTable* table = GetStringTable();
            if (!table)
                return nullptr;

            return table->GetString(stringId);
        }

        u32 GetStringCount() const { return GetStringTable()->GetCount(); }

        u32 GetItemId(const char* str) const { return GetItemIdImpl(Sections_PatriciaTree, str); }

        void DumpTree() const
        {
            for (u32 section = Sections_PatriciaTree; section <= Sections_Max; section++)
            {
                const PatriciaTree* tree = GetPatriciaTree((Sections)section);

                SEAD_PRINT("Section[%d] rootIdx(%d) count(%d)\n",
                            section, (u32)tree->rootIdx, (u32)tree->nodeTable.count);

                for (u32 i = 0; i < tree->nodeTable.count; i++)
                {
                    const PatriciaTree::Node* node = &tree->nodeTable.item[i];
                    const PatriciaTree::NodeData* data = &node->nodeData;
                    SEAD_PRINT("  idx(%4d) str(%4d) itemId(0x%08X) left(%4d) right(%4d) flags(%2d) bit(0x%04X) ",
                                i, (u32)data->stringId, (u32)data->itemId,
                                (u32)node->leftIdx, (u32)node->rightIdx, (u16)node->flags, (u16)node->bit);

                    if (data->stringId != 0xFFFFFFFF)
                        SEAD_PRINT("\"%s\"\n", GetString(data->stringId));
                    else
                        SEAD_PRINT("(null)\n");
                }
            }
        }

        const void* GetSection(Sections section) const
        {
            if (section > Sections_Max)
                return nullptr;

            return sead::PtrUtil::addOffset(this, toSection[section].offset);
        }

        const StringTable* GetStringTable() const
        {
            return reinterpret_cast<const StringTable*>(GetSection(Sections_StringTable));
        }
        const PatriciaTree* GetPatriciaTree(Sections section) const
        {
            return reinterpret_cast<const PatriciaTree*>(GetSection(section));
        }

        u32 GetItemIdImpl(Sections section, const char* str) const
        {
            const PatriciaTree* tree = GetPatriciaTree(section);
            const PatriciaTree::NodeData* nodeData = tree->GetNodeDataBy(str);
            if (!nodeData)
                return SoundArchive::INVALID_ID;

            const char* nodeDataStr = GetString(nodeData->stringId);
            SEAD_ASSERT(nodeDataStr);

            if (std::strcmp(str, nodeDataStr) == 0)
                return nodeData->itemId;

            return SoundArchive::INVALID_ID;
        }
    };
    static_assert(sizeof(StringBlockBody) == 0x10);

    struct StringBlock
    {
        ut::BinaryBlockHeader header;
        StringBlockBody body;
    };
    static_assert(sizeof(StringBlock) == 0x18);

    struct SoundInfo;
    struct BankInfo;
    struct PlayerInfo;
    struct SoundGroupInfo;
    struct GroupInfo;
    struct WaveArchiveInfo;
    struct FileInfo;
    struct SoundArchivePlayerInfo;

    struct InfoBlockBody
    {
        Util::Reference toSoundInfoReferenceTable;
        Util::Reference toSoundGroupInfoReferenceTable;
        Util::Reference toBankInfoReferenceTable;
        Util::Reference toWaveArchiveInfoReferenceTable;
        Util::Reference toGroupInfoReferenceTable;
        Util::Reference toPlayerInfoReferenceTable;
        Util::Reference toFileInfoReferenceTable;
        Util::Reference toSoundArchivePlayerInfo;

        u32 GetSoundCount() const       { return GetSoundInfoReferenceTable().count; }
        u32 GetBankCount() const        { return GetBankInfoReferenceTable().count; }
        u32 GetPlayerCount() const      { return GetPlayerInfoReferenceTable().count; }
        u32 GetSoundGroupCount() const  { return GetSoundGroupInfoReferenceTable().count; }
        u32 GetGroupCount() const       { return GetGroupInfoReferenceTable().count; }
        u32 GetWaveArchiveCount() const { return GetWaveArchiveInfoReferenceTable().count; }
        u32 GetFileCount() const        { return GetFileInfoReferenceTable().count; }

        const SoundInfo* GetSoundInfo(SoundArchive::ItemId soundId) const
        {
            if (GetItemTypeEnum(soundId) != ItemType_Sound)
                return nullptr;

            u32 index = Util::GetItemIndex(soundId);
            const Util::ReferenceTable& table = GetSoundInfoReferenceTable();

            if (index >= table.count)
                return nullptr;

            return reinterpret_cast<const SoundInfo*>(table.GetReferedItem(index));
        }

        const BankInfo* GetBankInfo(SoundArchive::ItemId bankId) const
        {
            if (GetItemTypeEnum(bankId) != ItemType_Bank)
                return nullptr;

            u32 index = Util::GetItemIndex(bankId);
            const Util::ReferenceTable& table = GetBankInfoReferenceTable();

            if (index >= table.count)
                return nullptr;

            return reinterpret_cast<const BankInfo*>(table.GetReferedItem(index));
        }

        const PlayerInfo* GetPlayerInfo(SoundArchive::ItemId playerId) const
        {
            if (GetItemTypeEnum(playerId) != ItemType_Player)
                return nullptr;

            u32 index = Util::GetItemIndex(playerId);
            const Util::ReferenceTable& table = GetPlayerInfoReferenceTable();

            if (index >= table.count)
                return nullptr;

            return reinterpret_cast<const PlayerInfo*>(table.GetReferedItem(index));
        }

        const SoundGroupInfo* GetSoundGroupInfo(SoundArchive::ItemId soundGroupId) const
        {
            if (GetItemTypeEnum(soundGroupId) != ItemType_SoundGroup)
                return nullptr;

            u32 index = Util::GetItemIndex(soundGroupId);
            const Util::ReferenceTable& table = GetSoundGroupInfoReferenceTable();

            if (index >= table.count)
                return nullptr;

            return reinterpret_cast<const SoundGroupInfo*>(table.GetReferedItem(index));
        }

        const GroupInfo* GetGroupInfo(SoundArchive::ItemId groupId) const
        {
            if (GetItemTypeEnum(groupId) != ItemType_Group)
                return nullptr;

            u32 index = Util::GetItemIndex(groupId);
            const Util::ReferenceTable& table = GetGroupInfoReferenceTable();

            if (index >= table.count)
                return nullptr;

            return reinterpret_cast<const GroupInfo*>(table.GetReferedItem(index));
        }

        const WaveArchiveInfo* GetWaveArchiveInfo(SoundArchive::ItemId warcId) const
        {
            if (GetItemTypeEnum(warcId) != ItemType_WaveArchive)
                return nullptr;

            u32 index = Util::GetItemIndex(warcId);
            const Util::ReferenceTable& table = GetWaveArchiveInfoReferenceTable();

            if (index >= table.count)
                return nullptr;

            return reinterpret_cast<const WaveArchiveInfo*>(table.GetReferedItem(index));
        }

        const FileInfo* GetFileInfo(SoundArchive::FileId fileId) const
        {
            u32 index = Util::GetItemIndex(fileId);
            const Util::ReferenceTable& table = GetFileInfoReferenceTable();

            if (index >= table.count)
                return nullptr;

            return reinterpret_cast<const FileInfo*>(table.GetReferedItem(index));
        }

        const SoundArchivePlayerInfo* GetSoundArchivePlayerInfo() const
        {
            return reinterpret_cast<const SoundArchivePlayerInfo*>(sead::PtrUtil::addOffset(this, toSoundArchivePlayerInfo.offset));
        }

        SoundArchive::FileId GetItemFileId(SoundArchive::ItemId id) const
        {
            SoundArchive::FileId fileId = SoundArchive::INVALID_ID;

            switch (Util::GetItemType(id))
            {
                case ItemType_Sound:
                {
                    const SoundInfo* info = GetSoundInfo(id);
                    if (info)
                        fileId = info->fileId;

                    break;
                }

                case ItemType_Bank:
                {
                    const BankInfo* info = GetBankInfo(id);
                    if (info)
                        fileId = info->fileId;

                    break;
                }

                case ItemType_WaveArchive:
                {
                    const WaveArchiveInfo* info = GetWaveArchiveInfo(id);
                    if (info)
                        fileId = info->fileId;

                    break;
                }

                case ItemType_Group:
                {
                    const GroupInfo* info = GetGroupInfo(id);
                    if (info)
                        fileId = info->fileId;

                    break;
                }

                case ItemType_SoundGroup:
                {
                    const SoundGroupInfo* info = GetSoundGroupInfo(id);
                    if (info)
                    {
                        SoundArchive::ItemId soundId = info->startId;
                        const SoundInfo* soundInfo = GetSoundInfo(soundId);
                        if (soundInfo)
                            fileId = soundInfo->fileId;
                    }

                    break;
                }

                case ItemType_Player:
                {
                    // No file ID
                    break;
                }
            }

            return fileId;
        }

        SoundArchive::StringId GetItemStringId(SoundArchive::ItemId id) const
        {
            SoundArchive::FileId stringId = SoundArchive::INVALID_ID;

            switch (Util::GetItemType(id))
            {
                case ItemType_Sound:
                {
                    const SoundInfo* info = GetSoundInfo(id);
                    if (info)
                        stringId = info->GetStringId();

                    break;
                }

                case ItemType_Bank:
                {
                    const BankInfo* info = GetBankInfo(id);
                    if (info)
                        stringId = info->GetStringId();

                    break;
                }

                case ItemType_WaveArchive:
                {
                    const WaveArchiveInfo* info = GetWaveArchiveInfo(id);
                    if (info)
                        stringId = info->GetStringId();

                    break;
                }

                case ItemType_SoundGroup:
                {
                    const SoundGroupInfo* info = GetSoundGroupInfo(id);
                    if (info)
                        stringId = info->GetStringId();

                    break;
                }

                case ItemType_Group:
                {
                    const GroupInfo* info = GetGroupInfo(id);
                    if (info)
                        stringId = info->GetStringId();

                    break;
                }

                case ItemType_Player:
                {
                    const PlayerInfo* info = GetPlayerInfo(id);
                    if (info)
                        stringId = info->GetStringId();

                    break;
                }
            }

            return stringId;
        }

        const Util::ReferenceTable& GetSoundInfoReferenceTable() const
        {
            return *reinterpret_cast<const Util::ReferenceTable*>(sead::PtrUtil::addOffset(this, toSoundInfoReferenceTable.offset));
        }
        const Util::ReferenceTable& GetBankInfoReferenceTable() const
        {
            return *reinterpret_cast<const Util::ReferenceTable*>(sead::PtrUtil::addOffset(this, toBankInfoReferenceTable.offset));
        }
        const Util::ReferenceTable& GetPlayerInfoReferenceTable() const
        {
            return *reinterpret_cast<const Util::ReferenceTable*>(sead::PtrUtil::addOffset(this, toPlayerInfoReferenceTable.offset));
        }
        const Util::ReferenceTable& GetSoundGroupInfoReferenceTable() const
        {
            return *reinterpret_cast<const Util::ReferenceTable*>(sead::PtrUtil::addOffset(this, toSoundGroupInfoReferenceTable.offset));
        }
        const Util::ReferenceTable& GetGroupInfoReferenceTable() const
        {
            return *reinterpret_cast<const Util::ReferenceTable*>(sead::PtrUtil::addOffset(this, toGroupInfoReferenceTable.offset));
        }
        const Util::ReferenceTable& GetWaveArchiveInfoReferenceTable() const
        {
            return *reinterpret_cast<const Util::ReferenceTable*>(sead::PtrUtil::addOffset(this, toWaveArchiveInfoReferenceTable.offset));
        }
        const Util::ReferenceTable& GetFileInfoReferenceTable() const
        {
            return *reinterpret_cast<const Util::ReferenceTable*>(sead::PtrUtil::addOffset(this, toFileInfoReferenceTable.offset));
        }
    };
    static_assert(sizeof(InfoBlockBody) == 0x40);

    struct InfoBlock
    {
        ut::BinaryBlockHeader header;
        InfoBlockBody body;
    };
    static_assert(sizeof(InfoBlock) == 0x48);

    struct StreamSoundInfo;
    struct WaveSoundInfo;
    struct SequenceSoundInfo;
    struct Sound3DInfo;

    struct SoundInfo
    {
        ut::ResU32 fileId;
        ut::ResU32 playerId;
        ut::ResU8 volume;
        ut::ResU8 remoteFilter;
        u16 padding1;
        Util::Reference toDetailSoundInfo;
        Util::BitFlag optionParameter;

        SoundArchive::SoundType GetSoundType() const
        {
            switch (toDetailSoundInfo.typeId)
            {
                case ElementType_SoundArchiveFile_StreamSoundInfo:
                    return SoundArchive::SOUND_TYPE_STRM;
                case ElementType_SoundArchiveFile_WaveSoundInfo:
                    return SoundArchive::SOUND_TYPE_WAVE;
                case ElementType_SoundArchiveFile_SequenceSoundInfo:
                    return SoundArchive::SOUND_TYPE_SEQ;
                default:
                    return SoundArchive::SOUND_TYPE_INVALID;
            }
        }

        const StreamSoundInfo& GetStreamSoundInfo() const
        {
            SEAD_ASSERT(toDetailSoundInfo.typeId == ElementType_SoundArchiveFile_StreamSoundInfo);

            return *reinterpret_cast<const StreamSoundInfo*>(sead::PtrUtil::addOffset(this, toDetailSoundInfo.offset));
        }
        const WaveSoundInfo& GetWaveSoundInfo() const
        {
            SEAD_ASSERT(toDetailSoundInfo.typeId == ElementType_SoundArchiveFile_WaveSoundInfo);

            return *reinterpret_cast<const WaveSoundInfo*>(sead::PtrUtil::addOffset(this, toDetailSoundInfo.offset));
        }
        const SequenceSoundInfo& GetSequenceSoundInfo() const
        {
            SEAD_ASSERT(toDetailSoundInfo.typeId == ElementType_SoundArchiveFile_SequenceSoundInfo);

            return *reinterpret_cast<const SequenceSoundInfo*>(sead::PtrUtil::addOffset(this, toDetailSoundInfo.offset));
        }

        const Sound3DInfo* GetSound3DInfo() const
        {
            u32 offset;
            bool result = optionParameter.GetValue(&offset, SOUND_INFO_OFFSET_TO_3D_PARAM);
            if (!result)
                return nullptr;

            return reinterpret_cast<const Sound3DInfo*>(sead::PtrUtil::addOffset(this, offset));
        }

        u32 GetStringId() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, SOUND_INFO_STRING_ID);
            if (!result)
                return DEFAULT_STRING_ID;

            return value;
        }

        PanMode GetPanMode() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, SOUND_INFO_PAN_PARAM);
            if (!result)
                return DEFAULT_PAN_MODE;

            return static_cast<PanMode>(Util::DevideBy8bit(value, 0));
        }
    
        PanCurve GetPanCurve() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, SOUND_INFO_PAN_PARAM);
            if (!result)
                return DEFAULT_PAN_CURVE;

            return static_cast<PanCurve>(Util::DevideBy8bit(value, 1));
        }

        u8 GetPlayerPriority() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, SOUND_INFO_PLAYER_PARAM);
            if (!result)
                return DEFAULT_PLAYER_PRIORITY;

            return Util::DevideBy8bit(value, 0);
        }

        u8 GetActorPlayerId() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, SOUND_INFO_PLAYER_PARAM);
            if (!result)
                return DEFAULT_ACTOR_PLAYER_ID;

            return Util::DevideBy8bit(value, 1);
        }

        u32 GetUserParam() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, SOUND_INFO_USER_PARAM);
            if (!result)
                return DEFAULT_USER_PARAM;

            return value;
        }

        bool ReadUserParam(s32 index, u32& value) const
        {
            SEAD_ASSERT(0 <= index && index < USER_PARAM_COUNT);
            return optionParameter.GetValue(&value, USER_PARAM_INDEX[index]);
        }

        bool IsFrontBypass() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, SOUND_INFO_OFFSET_TO_CTR_PARAM);
            if (!result)
                return DEFAULT_IS_FRONT_BYPASS;

            return value & (1 << 0);
        }
    };
    // Can't check size

    struct SendValue
    {
        ut::ResU8 mainSend;
        ut::ResU8 fxSend[AUX_BUS_NUM];
    };
    static_assert(sizeof(SendValue) == 0x4);

    struct StreamTrackInfo
    {
        u8 volume;
        u8 pan;
        u8 span;
        u8 flags;
        Util::Reference toGlobalChannelIndexTable;
        Util::Reference toSendValue;
        u8 lpfFreq;
        u8 biquadType;
        u8 biquadValue;
        u8 padding;

        u32 GetTrackChannelCount() const
        {
            return GetGlobalChannelIndexTable().GetCount();
        }
        u8 GetGlobalChannelIndex(u32 index) const
        {
            return GetGlobalChannelIndexTable().GetGlobalIndex(index);
        }

        const SendValue& GetSendValue() const
        {
            SEAD_ASSERT(toSendValue.typeId == ElementType_SoundArchiveFile_SendInfo);

            return *reinterpret_cast<const SendValue*>(sead::PtrUtil::addOffset(this, toSendValue.offset));
        }

        const StreamSoundFile::GlobalChannelIndexTable& GetGlobalChannelIndexTable() const
        {
            return *reinterpret_cast<const StreamSoundFile::GlobalChannelIndexTable*>(sead::PtrUtil::addOffset(this, toGlobalChannelIndexTable.offset));
        }
    };
    static_assert(sizeof(StreamTrackInfo) == 0x18);

    struct StreamTrackInfoTable
    {
        Util::ReferenceTable table;

        //? Custom implemented
        const StreamTrackInfo* GetTrackInfo(u32 index) const
        {
            return static_cast<const StreamTrackInfo*>(table.GetReferedItem(index, ElementType_SoundArchiveFile_StreamSoundTrackInfo));
        }

        u32 GetTrackCount() const { return table.count; }
    };
    // Can't check size

    struct StreamSoundExtension
    {
        ut::ResU32 streamTypeInfo;
        ut::ResU32 loopStartFrame;
        ut::ResU32 loopEndFrame;

        SoundArchive::StreamFileType GetStreamFileType() const
        {
            return static_cast<SoundArchive::StreamFileType>(Util::DevideBy8bit(streamTypeInfo, 0));
        }

        bool IsLoop() const
        {
            return Util::DevideBy8bit(streamTypeInfo, 1) == 1;
        }
    };
    static_assert(sizeof(StreamSoundExtension) == 0xC);

    struct StreamSoundInfo
    {
        ut::ResU16 allocateTrackFlags;
        ut::ResU16 allocateChannelCount;
        Util::Reference toTrackInfoTable;

        ut::ResF32 pitch;
        Util::Reference toSendValue;

        Util::Reference toStreamSoundExtension;

        ut::ResU32 prefetchFileId;

        const StreamTrackInfoTable* GetTrackInfoTable() const
        {
            if (toTrackInfoTable.typeId != ElementType_Table_ReferenceTable)
                return nullptr;

            return static_cast<const StreamTrackInfoTable*>(sead::PtrUtil::addOffset(this, toTrackInfoTable.offset));
        }

        const SendValue& GetSendValue() const
        {
            SEAD_ASSERT(toSendValue.typeId == ElementType_SoundArchiveFile_SendInfo);

            return *reinterpret_cast<const SoundArchiveFile::SendValue*>(sead::PtrUtil::addOffset(this, toSendValue.offset));
        }

        const StreamSoundExtension* GetStreamSoundExtension() const
        {
            if (!toStreamSoundExtension.IsValidOffset())
                return nullptr;

            if (toStreamSoundExtension.typeId != ElementType_SoundArchiveFile_StreamSoundExtensionInfo)
                return nullptr;

            return reinterpret_cast<const SoundArchiveFile::StreamSoundExtension*>(sead::PtrUtil::addOffset(this, toStreamSoundExtension.offset));
        }
    };
    static_assert(sizeof(StreamSoundInfo) == 0x24);

    struct WaveSoundInfo
    {
        ut::ResU32 index;
        ut::ResU32 allocateTrackCount;
        Util::BitFlag optionParameter;

        u8 GetChannelPriority() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, WAVE_SOUND_INFO_PRIORITY);
            if (!result)
                return DEFAULT_CHANNEL_PRIORITY;

            return Util::DevideBy8bit(value, 0);
        }

        u8 GetIsReleasePriorityFix() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, WAVE_SOUND_INFO_PRIORITY);
            if (!result)
                return DEFAULT_IS_RELEASE_PRIORITY_FIX;

            return Util::DevideBy8bit(value, 1);
        }
    };
    static_assert(sizeof(WaveSoundInfo) == 0xC);

    struct SequenceSoundInfo
    {
        Util::Reference toBankIdTable;
        ut::ResU32 allocateTrackFlags;
        Util::BitFlag optionParameter;

        void GetBankIds(u32* bankIds) const
        {
            const Util::Table<ut::ResU32>& table = GetBankIdTable();
            for (u32 i = 0; i < SoundArchive::SEQ_BANK_MAX; i++)
            {
                if (i >= table.count)
                    bankIds[i] = SoundArchive::INVALID_ID;
                else
                    bankIds[i] = table.item[i];
            }
        }

        u32 GetStartOffset() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, SEQ_SOUND_INFO_START_OFFSET);
            if (!result)
                return DEFAULT_SEQ_START_OFFSET;

            return value;
        }

        u8 GetChannelPriority() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, SEQ_SOUND_INFO_PRIORITY);
            if (!result)
                return DEFAULT_CHANNEL_PRIORITY;

            return Util::DevideBy8bit(value, 0);
        }

        bool IsReleasePriorityFix() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, SEQ_SOUND_INFO_PRIORITY);
            if (!result)
                return DEFAULT_CHANNEL_PRIORITY; // Bruh

            return Util::DevideBy8bit(value, 1) > 0;
        }

        const Util::Table<ut::ResU32>& GetBankIdTable() const
        {
            return *reinterpret_cast<const Util::Table<ut::ResU32>*>(sead::PtrUtil::addOffset(this, toBankIdTable.offset));
        }
    };
    static_assert(sizeof(SequenceSoundInfo) == 0x10);

    struct Sound3DInfo
    {
        ut::ResU32 flags;
        ut::ResF32 decayRatio;
        u8 decayCurve;
        u8 dopplerFactor;
        u8 padding[2];
        Util::BitFlag optionParameter;
    };
    // Can't check size

    struct BankInfo
    {
        ut::ResU32 fileId;
        Util::Reference toWaveArchiveItemIdTable;
        Util::BitFlag optionParameter;

        u32 GetStringId() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, BANK_INFO_STRING_ID);
            if (!result)
                return DEFAULT_STRING_ID;

            return value;
        }

        const Util::Table<ut::ResU32>* GetWaveArchiveItemIdTable() const
        {
            return reinterpret_cast<const Util::Table<ut::ResU32>*>(sead::PtrUtil::addOffset(this, toWaveArchiveItemIdTable.offset));
        }
    };
    static_assert(sizeof(BankInfo) == 0x10);

    struct PlayerInfo
    {
        ut::ResU32 playableSoundMax;
        Util::BitFlag optionParameter;

        u32 GetStringId() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, PLAYER_INFO_STRING_ID);
            if (!result)
                return DEFAULT_STRING_ID;

            return value;
        }

        u32 GetPlayerHeapSize() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, PLAYER_INFO_HEAP_SIZE);
            if (!result)
                return DEFAULT_PLAYER_HEAP_SIZE;

            return value;
        }
    };
    static_assert(sizeof(PlayerInfo) == 0x8);

    struct WaveSoundGroupInfo
    {
        Util::Reference toWaveArchiveItemIdTable;
        Util::BitFlag optionParameter;

        const Util::Table<ut::ResU32>* GetWaveArchiveItemIdTable() const
        {
            return reinterpret_cast<const Util::Table<ut::ResU32>*>(sead::PtrUtil::addOffset(this, toWaveArchiveItemIdTable.offset));
        }
    };
    static_assert(sizeof(WaveSoundGroupInfo) == 0xC);

    struct SoundGroupInfo
    {
        ut::ResU32 startId;
        ut::ResU32 endId;
        Util::Reference toFileIdTable;
        Util::Reference toDetailSoundGroupInfo;
        Util::BitFlag optionParameter;

        u32 GetStringId() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, SOUND_GROUP_INFO_STRING_ID);
            if (!result)
                return DEFAULT_STRING_ID;

            return value;
        }

        const Util::Table<ut::ResU32>* GetFileIdTable() const
        {
            return reinterpret_cast<const Util::Table<ut::ResU32>*>(sead::PtrUtil::addOffset(this, toFileIdTable.offset));
        }

        const WaveSoundGroupInfo* GetWaveSoundGroupInfo() const
        {
            if (toDetailSoundGroupInfo.typeId != ElementType_SoundArchiveFile_WaveSoundGroupInfo)
                return nullptr;

            return reinterpret_cast<const WaveSoundGroupInfo*>(sead::PtrUtil::addOffset(this, toDetailSoundGroupInfo.offset));
        }
    };
    static_assert(sizeof(SoundGroupInfo) == 0x1C);

    struct GroupInfo
    {
        ut::ResU32 fileId;
        Util::BitFlag optionParameter;

        u32 GetStringId() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, GROUP_INFO_STRING_ID);
            if (!result)
                return DEFAULT_STRING_ID;

            return value;
        }
    };
    static_assert(sizeof(GroupInfo) == 0x8);

    struct WaveArchiveInfo
    {
        ut::ResU32 fileId;
        bool isLoadIndividual;
        u8 padding1;
        u16 padding2;
        Util::BitFlag optionParameter;

        u32 GetStringId() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, WAVE_ARCHIVE_INFO_STRING_ID);
            if (!result)
                return DEFAULT_STRING_ID;

            return value;
        }

        u32 GetWaveCount() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, WAVE_ARCHIVE_INFO_WAVE_COUNT);
            if (!result)
                return DEFAULT_WARC_WAVE_COUNT;

            return value;
        }
    };
    static_assert(sizeof(WaveArchiveInfo) == 0xC);

    enum FileLocationType
    {
        FILE_LOCATION_TYPE_INTERNAL,
        FILE_LOCATION_TYPE_EXTERNAL,
        FILE_LOCATION_TYPE_NONE
    };

    struct InternalFileInfo
    {
        static const u32 INVALID_OFFSET = 0xFFFFFFFF;
        static const u32 INVALID_SIZE = 0xFFFFFFFF;

        Util::ReferenceWithSize toFileImageFromFileBlockBody;
        Util::Reference toAttachedGroupIdTable;

        u32 GetFileSize() const
        {
            return toFileImageFromFileBlockBody.size;
        }

        u32 GetOffsetFromFileBlockHead() const
        {
            if (toFileImageFromFileBlockBody.offset == INVALID_OFFSET)
                return INVALID_OFFSET;

            return toFileImageFromFileBlockBody.offset + sizeof(ut::BinaryBlockHeader);
        }

        const Util::Table<ut::ResU32>* GetAttachedGroupTable() const
        {
            return reinterpret_cast<const Util::Table<ut::ResU32>*>(sead::PtrUtil::addOffset(this, toAttachedGroupIdTable.offset));
        }
    };
    static_assert(sizeof(InternalFileInfo) == 0x14);

    struct ExternalFileInfo
    {
        char filePath[1];
    };
    // Can't check size

    struct FileInfo
    {
        Util::Reference toFileLocation;
        Util::BitFlag optionParameter;

        FileLocationType GetFileLocationType() const
        {
            switch (toFileLocation.typeId)
            {
                case ElementType_SoundArchiveFile_InternalFileInfo:
                    return FILE_LOCATION_TYPE_INTERNAL;

                case ElementType_SoundArchiveFile_ExternalFileInfo:
                    return FILE_LOCATION_TYPE_EXTERNAL;

                case 0:
                    return FILE_LOCATION_TYPE_NONE;

                default:
                    SEAD_ASSERT_MSG(false, "invalid file location type");
                    return FILE_LOCATION_TYPE_NONE;
            }
        }

        const InternalFileInfo* GetInternalFileInfo() const
        {
            if (GetFileLocationType() != FILE_LOCATION_TYPE_INTERNAL)
                return nullptr;

            return reinterpret_cast<const InternalFileInfo*>(sead::PtrUtil::addOffset(this, toFileLocation.offset));
        }

        const ExternalFileInfo* GetExternalFileInfo() const
        {
            if (GetFileLocationType() != FILE_LOCATION_TYPE_EXTERNAL)
                return nullptr;

            return reinterpret_cast<const ExternalFileInfo*>(sead::PtrUtil::addOffset(this, toFileLocation.offset));
        }
    };
    static_assert(sizeof(FileInfo) == 0xC);

    struct SoundArchivePlayerInfo
    {
        nw::ut::ResU16 sequenceSoundMax;
        nw::ut::ResU16 sequenceTrackMax;
        nw::ut::ResU16 streamSoundMax;
        nw::ut::ResU16 streamTrackMax;
        nw::ut::ResU16 streamChannelMax;
        nw::ut::ResU16 waveSoundMax;
        nw::ut::ResU16 waveTrackMax;
        u8 streamBufferTimes;
        u8 padding;
        nw::ut::ResU32 options;
    };
    static_assert(sizeof(SoundArchivePlayerInfo) == 0x14);

    struct FileBlockBody
    {
        // Only for getting the body address.
    };

    struct FileBlock
    {
        ut::BinaryBlockHeader header;
        FileBlockBody body;
    };
};

} } } // namespace nw::snd::internal
