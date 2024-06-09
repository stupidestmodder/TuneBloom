#pragma once

#include "MultiVoice.h"
#include "Channel.h"

namespace snd {

class WavePlayer
{
public:
    typedef void (*VoiceCallback)(void* arg);

    static const u32 cPriorityNoDrop = internal::Voice::cPriorityNoDrop;

public:
    WavePlayer();
    ~WavePlayer();

    bool initialize(s32 channelCount, u32 priority, VoiceCallback callback, void* callbackArg);
    void finalize();

    void start(snd::internal::WaveInfo* waveInfo = nullptr);
    void stop();
    void pause(bool isPauseOn);

    bool isActive() const;
    bool isRun() const;
    bool isPause() const;
    u32 getCurrentPlayingSample() const;

    void setWaveInfo(SampleFormat format, u32 sampleRate, s32 interpolationType);
    //void setAdpcmParam(s32 channel, const AdpcmParam& param);
    void appendWaveBuffer(s32 channel, WaveBuffer* buffer, bool lastFlag);

    void setPriority(u32 priority);
    void setVolume(f32 volume);
    void setPitch(f32 pitch);
    void setPanMode(PanMode mode);
    void setPanCurve(PanCurve curve);
    void setPan(f32 pan);
    void setLpfFreq(f32 lpfFreq);
    void setBiquadFilter(s32 type, f32 value);

    void setOutputLine(u32 lineFlag);

    void setOutVolume(f32 volume);
    void setMainSend(f32 send);
    void setFxSend(AuxBus bus, f32 send);

private:
    static void MultiVoiceCallbackFunc(internal::driver::MultiVoice* voice, internal::driver::MultiVoice::VoiceCallbackStatus status, void* arg);

private:
    internal::driver::MultiVoice* mMultiVoice;
    VoiceCallback mCallback;
    void* mCallbackArg; 

    internal::OutputParam mParam;

    snd::internal::driver::Channel* mChannel;
};

} // namespace snd
