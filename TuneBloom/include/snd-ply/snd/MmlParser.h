#pragma once

#include "SequenceTrack.h"

class MmlParser
{
public:
    static const s32 PAN_CENTER = 64;
    static const s32 SURROUND_PAN_CENTER = 64;
    static const s32 TEMPO_MIN = 0;
    static const s32 TEMPO_MAX = 1023;

    enum SeqArgType
    {
        SEQ_ARG_NONE,
        SEQ_ARG_U8,
        SEQ_ARG_S16,
        SEQ_ARG_VMIDI,
        SEQ_ARG_RANDOM,
        SEQ_ARG_VARIABLE
    };

public:
    SequenceTrack::ParseResult parse(SequenceTrack* track, bool doNoteOn) const;

    void commandProc(
        SequenceTrack* track,
        u32 command,
        s32 commandArg1,
        s32 commandArg2
    ) const;

    void noteOnCommandProc(
        SequenceTrack* track,
        int key,
        int velocity,
        s32 length,
        bool tieFlag
    ) const;

    static u8   ReadByte( const u8** ptr ) { return *(*ptr)++; }
    static void UnreadByte( const u8** ptr ) { --(*ptr); }
    static u16  Read16( const u8** ptr );
    static u32  Read24( const u8** ptr );
    static s32  ReadVar( const u8** ptr );
    static s32  ReadArg( const u8** ptr, SequenceSoundPlayer* player, SequenceTrack* track, SeqArgType argType );
    static volatile s16* GetVariablePtr( SequenceSoundPlayer* player, SequenceTrack* track, int varNo );
};
