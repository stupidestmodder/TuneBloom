#pragma once

#include "Channel.h"
#include "CurveLfo.h"
#include "MoveValue.h"

class SequenceSoundPlayer;
class MmlParser;

class SequenceTrack
{
public:
    static const s32 cCallStackDepth = 10;

    struct ParserTrackParam
    {
        const u8* baseAddr;
        const u8* currentAddr;

        const u8* currentCmdAddr; // Custom

        bool cmpFlag;
        bool noteWaitFlag;
        bool tieFlag;
        bool monophonicFlag;

        struct CallStack
        {
            u8 loopFlag;
            u8 loopCount;
            u8 padding[2];
            const u8* address;
        };

        CallStack callStack[cCallStackDepth];
        u8 callStackDepth;
        bool frontBypassFlag;
        bool muteFlag;
        bool silenceFlag;

        s32 wait;

        s32 waitAmount; // Custom

        bool noteFinishWait;
        bool portaFlag;
        bool damperFlag;
        u8 bankIndex;

        s32 prgNo;

        f32 sweepPitch;

        snd::internal::MoveValue<u8, s16> volume;
        snd::internal::MoveValue<u8, s16> volume2;
        snd::internal::MoveValue<s8, s16> pan;
        snd::internal::MoveValue<s8, s16> surroundPan;
        snd::internal::MoveValue<s8, s16> pitchBend;

        snd::internal::CurveLfoParam lfoParam[snd::internal::driver::Channel::cModCount];
        u8 lfoTarget[snd::internal::driver::Channel::cModCount];

        u8 velocityRange;
        u8 bendRange;
        s8 initPan;
        u8 padding;

        s8 transpose;
        u8 priority;
        u8 portaKey;
        u8 portaTime;

        u8 attack;
        u8 decay;
        u8 sustain;
        u8 release;

        s16 envHold;
        s8 biquadType;
        u8 mainSend;

        u8 fxSend[(u32)snd::AuxBus::Num];
        u8 padding2;

        f32 lpfFreq;
        f32 biquadValue;
        s32 outputLine;
    };

    enum ParseResult
    {
        PARSE_RESULT_CONTINUE,
        PARSE_RESULT_FINISH
    };

    static const s32 cDefaultPriority   = 64;
    static const s32 cDefaultBendRange  = 2;
    static const s32 cDefaultPortaKey   = 60; /* cn4 */
    static const s32 cInvalidEnvelope   = 0xFF;
    static const s32 cMaxEnvelopeValue  = 0x7F;

    static const s32 cTrackVariableNum = 16;
    static const s32 cPauseReleaseValue = 127;

    static void channelCallbackFunc(snd::internal::driver::Channel* dropChannel, snd::internal::driver::Channel::ChannelCallbackStatus status, void* userData);

public:
    SequenceTrack();

    void setSeqData(const void* seqBase, s32 seqOffset);
    void initParam();

    bool isOpened() const { return mOpenFlag; }
    void setPlayerTrackNo(s32 trackNo);
    void open();
    void close(bool stop = false);
    void updateChannelParam();
    void updateChannelLength();
    void updateChannelRelease(snd::internal::driver::Channel* channel);
    s32 parseNextTick(bool doNoteOn);

    snd::internal::driver::Channel* noteOn(s32 key, s32 velocity, s32 length, bool tieFlag);
    void stopAllChannel();
    void releaseAllChannel(s32 release, bool stop = false);
    void freeAllChannel();
    void pauseAllChannel(bool flag);

    const ParserTrackParam& getParserTrackParam() const { return mParserTrackParam; }
    ParserTrackParam& getParserTrackParam() { return mParserTrackParam; }

    void setVolume(f32 volume) { mExtVolume = volume; }

    f32 getVolume() const { return mExtVolume; }

    void setSequenceSoundPlayer(SequenceSoundPlayer* player) { mSequenceSoundPlayer = player; }
    const SequenceSoundPlayer* getSequenceSoundPlayer() const { return mSequenceSoundPlayer; }
    SequenceSoundPlayer* getSequenceSoundPlayer() { return mSequenceSoundPlayer; }

    void setMmlParser(const MmlParser* parser) { mParser = parser; }
    const MmlParser* getMmlParser() const { return mParser; }

    s16 getTrackVariable(s32 varNo) const
    {
        SEAD_ASSERT(0 <= varNo && varNo < cTrackVariableNum);

        return mTrackVariable[varNo];
    }

    void setTrackVariable(s32 varNo, s16 var)
    {
        SEAD_ASSERT(0 <= varNo && varNo < cTrackVariableNum);

        mTrackVariable[varNo] = var;
    }

    volatile s16* getVariablePtr(s32 varNo)
    {
        SEAD_ASSERT(0 <= varNo && varNo < cTrackVariableNum);

        if (varNo < cTrackVariableNum)
            return &mTrackVariable[varNo];
        else
            return nullptr;
    }

protected:
    ParseResult parse(bool doNoteOn);

    snd::internal::driver::Channel* getLastChannel() const { return mChannelList; }
    void addChannel(snd::internal::driver::Channel* channel);

private:
    u8 mPlayerTrackNo;
    bool mOpenFlag;
    f32 mExtVolume;
    f32 mExtPitch;
    f32 mPanRange;

    snd::internal::OutputParam mParam;

    ParserTrackParam mParserTrackParam;
    volatile s16 mTrackVariable[cTrackVariableNum];

    SequenceSoundPlayer* mSequenceSoundPlayer;
    snd::internal::driver::Channel* mChannelList;

    const MmlParser* mParser;
};
