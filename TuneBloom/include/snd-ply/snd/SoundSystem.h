#pragma once

#include "Global.h"

#include <heap/seadHeap.h>

namespace snd {

namespace internal {

class VoiceRenderer;

} // namespace internal

class SoundSystem
{
public:
    struct SoundSystemParam
    {
        SoundSystemParam()
            : sound_thread_priority(cDefaultSoundThreadPriority)
            , sound_thread_stack_size(cDefaultSoundThreadStackSize)
            , sound_thread_command_buffer_size(cDefaultSoundThreadCommandBufferSize)

            , task_thread_priority(cDefaultTaskThreadPriority)
            , task_thread_stack_size(cDefaultTaskThreadStackSize)
            , task_thread_command_buffer_size(cDefaultTaskThreadCommandBufferSize)

            , enable_get_sound_thread_tick(true)
            , enable_visualization(false)

            , voice_synthesize_buffer_count(5)
        {
        }

        s32 sound_thread_priority;
        u32 sound_thread_stack_size;
        u32 sound_thread_command_buffer_size;

        s32 task_thread_priority;
        u32 task_thread_stack_size;
        u32 task_thread_command_buffer_size;

        bool enable_get_sound_thread_tick;
        bool enable_visualization;

        u32 voice_synthesize_buffer_count;

        static const s32 cDefaultSoundThreadPriority = 31;
        static const u32 cDefaultSoundThreadStackSize = 16 * 1024;
        static const u32 cDefaultSoundThreadCommandBufferSize = 128 * 1024;

        static const s32 cDefaultTaskThreadPriority = 30;
        static const u32 cDefaultTaskThreadStackSize = 16 * 1024;
        static const u32 cDefaultTaskThreadCommandBufferSize = 8 * 1024;
    };

    static const u32 cMaxVoiceCount;
    static const u32 cSamplePerFrame;

private:
    SoundSystem();
    ~SoundSystem();

public:
    static void initialize(const SoundSystemParam& param, sead::Heap* heap);
    static void finalize();

    static bool isInitialized()
    {
        return sIsInitialized;
    }

    static void detail_initializeDriverCommandMgr(const SoundSystemParam& param, sead::Heap* heap);
    static void detail_finalizeDriverCommandMgr();

    static void setSoundFrameUserCallback(SoundFrameUserCallback callback, void* arg);
    static void clearSoundFrameUserCallback();

    static void lockSoundThread();
    static void unlockSoundThread();

    static void enterSleep();
    static void leaveSleep();

    static bool detail_isStreamLoadWait()
    {
        return sIsStreamLoadWait;
    }

    static bool detail_isInitializedVoiceRenderer()
    {
        return sIsInitializedVoiceRenderer;
    }

    static void detail_setStreamOpenFailureHaltEnabled(bool isEnabled)
    {
        sIsStreamOpenFailureHalt = isEnabled;
    }

    static bool detail_isStreamOpenFailureHaltEnabled()
    {
        return sIsStreamOpenFailureHalt;
    }

    static bool isEnableVisualization()
    {
        return sIsEnableVisualization;
    }

    static void setEnableVisualization(bool enable)
    {
        sIsEnableVisualization = enable;
    }

    static f32* getWave();
    static f32* calcFFT();

private:
    static u32 sMaxVoiceCount;

    static u32 sLoadThreadStackSize;
    static u32 sSoundThreadStackSize;

    static bool sIsInitialized;
    static bool sIsStreamLoadWait;
    static bool sIsEnterSleep;
    static bool sIsInitializedDriverCommandMgr;
    static bool sIsInitializedVoiceRenderer;
    static bool sIsStreamOpenFailureHalt;
    static bool sIsEnableVisualization;

    static internal::VoiceRenderer* sVoiceRenderer;
    static u32 sSoundThreadCommandBufferSize;
    static u32 sTaskThreadCommandBufferSize;
    static u32 sVoiceCommandBufferSize;
    static u32 sSynthesizeBufferSize;
};

} // namespace snd
