#pragma once

#include "BasicSoundPlayer.h"
#include "Channel.h"
#include "SoundThread.h"

#include "snd/snd_WaveFileReader.h"
#include "snd/snd_WaveSoundFileReader.h"

#include <bfsar/BankFile.h>
#include <bfsar/WaveFile.h>

class WaveSoundPlayer : public BasicSoundPlayer, public snd::internal::driver::SoundThread::PlayerCallback
{
public:
    struct PrepareArg
    {
        PrepareArg()
            : wsdFile(nullptr)
            , waveFile(nullptr)
            , useContextInfo(false)
        {
        }

        const void* wsdFile;
        const void* waveFile;
        bool useContextInfo;
        nw::snd::internal::WaveFileReader::OffsetContextInfo contextInfo;
    };

    static void channelCallbackFunc(snd::internal::driver::Channel* dropChannel, snd::internal::driver::Channel::ChannelCallbackStatus, void* userData);

public:
    WaveSoundPlayer();

    void onUpdateFrameSoundThread() override
    {
        update();
    }

    void onUpdateFrameSoundThreadWithAudioFrameFrequency() override
    {
        if (mUpdateType == snd::UpdateType::AudioFrame)
            update();
    }

    void init();
    void deinit(bool stop = false);
    void prepare(s32 index, const PrepareArg& arg, snd::UpdateType updateType = snd::UpdateType::AudioFrame);
    void prepare(const WaveFile& waveFile, s32 channelIdx = -1, snd::UpdateType updateType = snd::UpdateType::AudioFrame);
    void setBankNoteInfo(u8 key, u8 velocity, const BankFile::VelocityRegion& velocityRegion);

    void pause(bool flag) override;

    void update();

    bool isChannelActive() const { return mChannel && mChannel->isActive(); }

    bool startChannel(const nw::snd::internal::WaveInfo* waveInfoPtr = nullptr);
    void closeChannel(bool stop = false);
    void updateChannel();

    s32 getPlaySamplePosition(bool isOriginalSamplePosition) const;

    u32 getSampleRate() const
    {
        return mSampleRate;
    }

    u32 getSampleCount() const
    {
        return mSampleCount;
    }

protected:
    bool mIsRegisterPlayerCallback;

    bool mWavePlayFlag;
    bool mReleasePriorityFixFlag;

    f32 mPanRange;

    const void* mWsdFile;
    const void* mWaveFile;
    s32 mWaveSoundIndex;

    nw::snd::internal::WaveSoundInfo mWaveSoundInfo;
    snd::internal::driver::Channel* mChannel;

    snd::UpdateType mUpdateType;

    u32 mSampleRate;
    u32 mSampleCount;
};
