#pragma once

#include <basis/seadTypes.h>

namespace nw { namespace snd { namespace internal {

enum ElementType
{
    ElementType_Category_Tables                              = 0x0100,
    ElementType_Table_EmbeddingTable                         = ElementType_Category_Tables + 0,
    ElementType_Table_ReferenceTable                         = ElementType_Category_Tables + 1,
    ElementType_Table_ReferenceWithSizeTable                 = ElementType_Category_Tables + 2,

    ElementType_Category_Parameters                          = 0x0200,
    ElementType_Parameter_Sound3D                            = ElementType_Category_Parameters + 0,
    ElementType_Parameter_Sends                              = ElementType_Category_Parameters + 1,
    ElementType_Parameter_Envelope                           = ElementType_Category_Parameters + 2,
    ElementType_Parameter_AdshrEnvelope                      = ElementType_Category_Parameters + 3,

    ElementType_Category_Codecs                              = 0x0300,
    ElementType_Codec_DspAdpcmInfo                           = ElementType_Category_Codecs + 0,
    ElementType_Codec_ImaAdpcmInfo                           = ElementType_Category_Codecs + 1,

    ElementType_Category_General                             = 0x1F00,
    ElementType_General_ByteStream                           = ElementType_Category_General + 0,
    ElementType_General_String                               = ElementType_Category_General + 1,

    ElementType_Category_SoundArchiveFile_Blocks             = 0x2000,
    ElementType_SoundArchiveFile_StringBlock                 = ElementType_Category_SoundArchiveFile_Blocks + 0,
    ElementType_SoundArchiveFile_InfoBlock                   = ElementType_Category_SoundArchiveFile_Blocks + 1,
    ElementType_SoundArchiveFile_FileBlock                   = ElementType_Category_SoundArchiveFile_Blocks + 2,

    ElementType_Category_SoundArchiveFile_InfoSections       = 0x2100,
    ElementType_SoundArchiveFile_SoundInfoSection            = ElementType_Category_SoundArchiveFile_InfoSections + 0,
    ElementType_SoundArchiveFile_BankInfoSection             = ElementType_Category_SoundArchiveFile_InfoSections + 1,
    ElementType_SoundArchiveFile_PlayerInfoSection           = ElementType_Category_SoundArchiveFile_InfoSections + 2,
    ElementType_SoundArchiveFile_WaveArchiveInfoSection      = ElementType_Category_SoundArchiveFile_InfoSections + 3,
    ElementType_SoundArchiveFile_SoundGroupInfoSection       = ElementType_Category_SoundArchiveFile_InfoSections + 4,
    ElementType_SoundArchiveFile_GroupInfoSection            = ElementType_Category_SoundArchiveFile_InfoSections + 5,
    ElementType_SoundArchiveFile_FileInfoSection             = ElementType_Category_SoundArchiveFile_InfoSections + 6,

    ElementType_Category_SoundArchiveFile_ItemInfos          = 0x2200,
    ElementType_SoundArchiveFile_SoundInfo                   = ElementType_Category_SoundArchiveFile_ItemInfos + 0,
    ElementType_SoundArchiveFile_StreamSoundInfo             = ElementType_Category_SoundArchiveFile_ItemInfos + 1,
    ElementType_SoundArchiveFile_WaveSoundInfo               = ElementType_Category_SoundArchiveFile_ItemInfos + 2,
    ElementType_SoundArchiveFile_SequenceSoundInfo           = ElementType_Category_SoundArchiveFile_ItemInfos + 3,
    ElementType_SoundArchiveFile_SoundGroupInfo              = ElementType_Category_SoundArchiveFile_ItemInfos + 4,
    ElementType_SoundArchiveFile_WaveSoundGroupInfo          = ElementType_Category_SoundArchiveFile_ItemInfos + 5,
    ElementType_SoundArchiveFile_BankInfo                    = ElementType_Category_SoundArchiveFile_ItemInfos + 6,
    ElementType_SoundArchiveFile_WaveArchiveInfo             = ElementType_Category_SoundArchiveFile_ItemInfos + 7,
    ElementType_SoundArchiveFile_GroupInfo                   = ElementType_Category_SoundArchiveFile_ItemInfos + 8,
    ElementType_SoundArchiveFile_PlayerInfo                  = ElementType_Category_SoundArchiveFile_ItemInfos + 9,
    ElementType_SoundArchiveFile_FileInfo                    = ElementType_Category_SoundArchiveFile_ItemInfos + 10,
    ElementType_SoundArchiveFile_SoundArchivePlayerInfo      = ElementType_Category_SoundArchiveFile_ItemInfos + 11,
    ElementType_SoundArchiveFile_InternalFileInfo            = ElementType_Category_SoundArchiveFile_ItemInfos + 12,
    ElementType_SoundArchiveFile_ExternalFileInfo            = ElementType_Category_SoundArchiveFile_ItemInfos + 13,
    ElementType_SoundArchiveFile_StreamSoundTrackInfo        = ElementType_Category_SoundArchiveFile_ItemInfos + 14,
    ElementType_SoundArchiveFile_SendInfo                    = ElementType_Category_SoundArchiveFile_ItemInfos + 15,
    ElementType_SoundArchiveFile_StreamSoundExtensionInfo    = ElementType_Category_SoundArchiveFile_ItemInfos + 16,

    ElementType_Category_SoundArchiveFile_Parameters         = 0x2300,

    ElementType_Category_SoundArchiveFile_General            = 0x2400,
    ElementType_SoundArchiveFile_StringTable                 = ElementType_Category_SoundArchiveFile_General + 0,
    ElementType_SoundArchiveFile_PatriciaTree                = ElementType_Category_SoundArchiveFile_General + 1,

    ElementType_Category_StreamSoundFile_Blocks              = 0x4000,
    ElementType_StreamSoundFile_InfoBlock                    = ElementType_Category_StreamSoundFile_Blocks + 0,
    ElementType_StreamSoundFile_SeekBlock                    = ElementType_Category_StreamSoundFile_Blocks + 1,
    ElementType_StreamSoundFile_DataBlock                    = ElementType_Category_StreamSoundFile_Blocks + 2,
    ElementType_StreamSoundFile_RegionBlock                  = ElementType_Category_StreamSoundFile_Blocks + 3,

    ElementType_Category_StreamSoundFile_ItemInfos           = 0x4100,
    ElementType_StreamSoundFile_StreamSoundInfo              = ElementType_Category_StreamSoundFile_ItemInfos + 0,
    ElementType_StreamSoundFile_TrackInfo                    = ElementType_Category_StreamSoundFile_ItemInfos + 1,
    ElementType_StreamSoundFile_ChannelInfo                  = ElementType_Category_StreamSoundFile_ItemInfos + 2,

    ElementType_Category_WaveSoundFile_Blocks                = 0x4800,

    ElementType_Category_WaveSoundFile_ItemInfos             = 0x4900,
    ElementType_WaveSoundFile_WaveSoundMetaData              = ElementType_Category_WaveSoundFile_ItemInfos + 0,
    ElementType_WaveSoundFile_WaveSoundInfo                  = ElementType_Category_WaveSoundFile_ItemInfos + 1,
    ElementType_WaveSoundFile_NoteInfo                       = ElementType_Category_WaveSoundFile_ItemInfos + 2,
    ElementType_WaveSoundFile_TrackInfo                      = ElementType_Category_WaveSoundFile_ItemInfos + 3,
    ElementType_WaveSoundFile_NoteEvent                      = ElementType_Category_WaveSoundFile_ItemInfos + 4,

    ElementType_Category_SequenceSoundFile_Blocks            = 0x5000,
    ElementType_SequenceSoundFile_DataBlock                  = ElementType_Category_SequenceSoundFile_Blocks + 0,
    ElementType_SequenceSoundFile_LabelBlock                 = ElementType_Category_SequenceSoundFile_Blocks + 1,

    ElementType_Category_SequenceSoundFile_ItemInfos         = 0x5100,
    ElementType_SequenceSoundFile_LabelInfo                  = ElementType_Category_SequenceSoundFile_ItemInfos + 0,

    ElementType_Category_BankFile_Blocks                     = 0x5800,
    ElementType_BankFile_InfoBlock                           = ElementType_Category_BankFile_Blocks + 0,

    ElementType_Category_BankFile_Items                      = 0x5900,
    ElementType_BankFile_InstrumentInfo                      = ElementType_Category_BankFile_Items + 0,
    ElementType_BankFile_KeyRegionInfo                       = ElementType_Category_BankFile_Items + 1,
    ElementType_BankFile_VelocityRegionInfo                  = ElementType_Category_BankFile_Items + 2,
    ElementType_BankFile_NullInfo                            = ElementType_Category_BankFile_Items + 3,

    ElementType_Category_BankFile_ItemTables                 = 0x6000,
    ElementType_BankFile_DirectReferenceTable                = ElementType_Category_BankFile_ItemTables + 0,
    ElementType_BankFile_RangeReferenceTable                 = ElementType_Category_BankFile_ItemTables + 1,
    ElementType_BankFile_IndexReferenceTable                 = ElementType_Category_BankFile_ItemTables + 2,

    ElementType_Category_WaveArchiveFile_Blocks              = 0x6800,
    ElementType_WaveArchiveFile_InfoBlock                    = ElementType_Category_WaveArchiveFile_Blocks + 0,
    ElementType_WaveArchiveFile_FileBlock                    = ElementType_Category_WaveArchiveFile_Blocks + 1,

    ElementType_WaveSoundFile_InfoBlock                      = ElementType_Category_WaveArchiveFile_Blocks + 0,

    ElementType_Category_WaveFile_Blocks                     = 0x7000,
    ElementType_WaveFile_InfoBlock                           = ElementType_Category_WaveFile_Blocks + 0,
    ElementType_WaveFile_DataBlock                           = ElementType_Category_WaveFile_Blocks + 1,

    ElementType_Category_WaveFile_ItemInfos                  = 0x7100,
    ElementType_WaveFile_ChannelInfo                         = ElementType_Category_WaveFile_ItemInfos + 0,

    ElementType_Category_GroupFile_Blocks                    = 0x7800,
    ElementType_GroupFile_InfoBlock                          = ElementType_Category_GroupFile_Blocks + 0,
    ElementType_GroupFile_FileBlock                          = ElementType_Category_GroupFile_Blocks + 1,
    ElementType_GroupFile_InfoExBlock                        = ElementType_Category_GroupFile_Blocks + 2,

    ElementType_Category_GroupFile_ItemInfos                 = 0x7900,
    ElementType_GroupFile_GroupItemInfo                      = ElementType_Category_GroupFile_ItemInfos + 0,
    ElementType_GroupFile_GroupItemInfoEx                    = ElementType_Category_GroupFile_ItemInfos + 1,

    ElementType_Category_AnimSoundFile_Blocks                = 0x8000,
    ElementType_AnimSoundFile_DataBlock                      = ElementType_Category_AnimSoundFile_Blocks + 0,

    ElementType_Category_AnimSoundFile_Items                 = 0x8100,
    ElementType_AnimSoundFile_EventInfo                      = ElementType_Category_AnimSoundFile_Items + 0,
};

} } } // namespace nw::snd::internal