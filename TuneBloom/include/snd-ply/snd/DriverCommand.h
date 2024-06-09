#pragma once

#include <heap/seadDisposer.h>

#include "CommandMgr.h"
#include "Command.h"
#include "Global.h"

namespace snd { namespace internal {

namespace driver {

class MultiVoice;

} // namespace driver

class DriverCommandId
{
public:
    enum Id
    {
        Dummy,

        AllVoicesSync,

        VoicePlay,
        VoiceWaveInfo,
        //VoiceAppendWaveBuffer,
        //VoicePriority,
        //VoiceVolume,
        //VoicePitch,
        //VoicePanMode,
        //VoicePanCurve,
        //VoicePan,
        //VoiceLpf,
        //VoiceBiquad,
        //VoiceOutoutLine
        //VoiceOutVolume,
    };
};

struct DriverCommandAllVoicesSync : public Command
{
    u32 syncFlag;
};

// The Voice to MultiVoice operation

struct DriverCommandVoice : public Command
{
    driver::MultiVoice* voice;
};

struct DriverCommandVoicePlay : public DriverCommandVoice
{
    enum class State
    {
        Start,
        Stop,
        PauseOn,
        PauseOff
    };

    State state;
};

struct DriverCommandVoiceWaveInfo : public DriverCommandVoice
{
    SampleFormat format;
    u32 sampleRate;
    s32 interpolationType;
};

class DriverCommandImpl : public CommandMgr
{
protected:
    DriverCommandImpl();
    ~DriverCommandImpl() override;

    static void processCommandList(Command* commandList);
    static void requestProcessCommand();

public:
    void initialize(u32 commandBufferSize, sead::Heap* heap);
};

class DriverCommand : public DriverCommandImpl
{
    SEAD_SINGLETON_DISPOSER(DriverCommand);

private:
    DriverCommand()
    {
    }
};

class DriverCommandForTaskThread : public DriverCommandImpl
{
    SEAD_SINGLETON_DISPOSER(DriverCommandForTaskThread);

private:
    DriverCommandForTaskThread()
    {
    }
};

} } // namespace snd::internal
