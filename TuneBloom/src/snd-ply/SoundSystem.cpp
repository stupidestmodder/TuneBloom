#include "snd/SoundSystem.h"

#include "snd/SoundThread.h"
#include "snd/VoiceImpl.h"
#include "snd/DriverCommand.h"
#include "snd/TaskMgr.h"
#include "snd/MultiVoiceMgr.h"
#include "snd/ChannelMgr.h"
#include "snd/DisposeCallbackMgr.h"
#include "snd/TaskThread.h"
#include "snd/CurveLfo.h"
#include "snd/FFT.h"

#include <heap/seadHeapMgr.h>
#include <thread/seadThreadUtil.h>

#define MINIAUDIO_IMPLEMENTATION
#include "snd/miniaudio.h"

namespace snd {

const u32 SoundSystem::cMaxVoiceCount = internal::driver::HardwareMgr::cMaxVoiceCount;
const u32 SoundSystem::cSamplePerFrame = internal::driver::HardwareMgr::cSamplePerFrame;

u32 SoundSystem::sMaxVoiceCount = SoundSystem::cMaxVoiceCount;

u32 SoundSystem::sLoadThreadStackSize = 0;
u32 SoundSystem::sSoundThreadStackSize = 0;

bool SoundSystem::sIsInitialized = false;
bool SoundSystem::sIsStreamLoadWait = false;
bool SoundSystem::sIsEnterSleep = false;
bool SoundSystem::sIsInitializedDriverCommandMgr = false;
bool SoundSystem::sIsInitializedVoiceRenderer = false;
bool SoundSystem::sIsStreamOpenFailureHalt = true;
bool SoundSystem::sIsEnableVisualization = false;

internal::VoiceRenderer* SoundSystem::sVoiceRenderer = nullptr;
u32 SoundSystem::sSoundThreadCommandBufferSize = 0;
u32 SoundSystem::sTaskThreadCommandBufferSize = 0;
u32 SoundSystem::sVoiceCommandBufferSize = 0;
u32 SoundSystem::sSynthesizeBufferSize = 0;

//static void OboeInit(sead::Heap* heap);
//static void OboeQuit();

static void InitSDK(sead::Heap* heap);
static void QuitSDK();

void SoundSystem::initialize(const SoundSystemParam& param, sead::Heap* heap)
{
    // Multiple initialization check
    if (sIsInitialized)
        return;

    internal::driver::HardwareMgr::createInstance(heap);
    internal::driver::SoundThread::createInstance(heap);
    internal::TaskMgr::createInstance(heap);
    internal::driver::MultiVoiceMgr::createInstance(heap);
    internal::driver::ChannelMgr::createInstance(heap);
    internal::driver::DisposeCallbackMgr::createInstance(heap);
    internal::TaskThread::createInstance(heap);

    sIsEnableVisualization = param.enable_visualization;

    // Initializes VoiceRenderer.
    if (!sIsInitializedVoiceRenderer)
    {
        sVoiceRenderer = new(heap) internal::VoiceRenderer();
        //sVoiceRenderer = (internal::VoiceRenderer*)malloc(sizeof(internal::VoiceRenderer));
        //new(sVoiceRenderer) internal::VoiceRenderer();

        sVoiceRenderer->initialize(param.voice_synthesize_buffer_count, heap);

        internal::driver::SoundThread::instance()->detail_setVoiceRenderer(sVoiceRenderer);

        sSynthesizeBufferSize = internal::VoiceSynthesizeBuffer::getSynthesizeBufferSize() * param.voice_synthesize_buffer_count;

        sIsInitializedVoiceRenderer = true;
    }

    // Initializes the command buffer.
    SoundSystem::detail_initializeDriverCommandMgr(param, heap);

    // Initializes SDK-related data.
    internal::driver::HardwareMgr::instance()->initialize();

    // Prepares the snd sound thread.
    {
        sSoundThreadStackSize = param.sound_thread_stack_size;
        internal::driver::SoundThread::instance()->initialize();
    }

    // Prepares the snd sound data load thread.
    {
        internal::TaskMgr::instance()->initialize(heap);
        sLoadThreadStackSize = param.task_thread_stack_size;
    }

    // Initializes MultiVoiceManager.
    {
        internal::driver::MultiVoiceMgr::instance()->initialize(sMaxVoiceCount, heap);
    }

    // ChannelManager initialization
    {
        internal::driver::ChannelMgr::instance()->initialize(sMaxVoiceCount, heap);
    }

    // Initializes SequenceSoundPlayer.
    //internal::driver::SequenceSoundPlayer::initSequenceSoundPlayer();

    u16 attribute = 0;

    // Launches the data load thread.
    bool result = internal::TaskThread::instance()->create(sead::ThreadUtil::ConvertPrioritySeadToPlatform(param.task_thread_priority), sLoadThreadStackSize, attribute, heap);
    SEAD_ASSERT(result);

    // Sound thread start
    result = internal::driver::SoundThread::instance()->createSoundThread(sead::ThreadUtil::ConvertPrioritySeadToPlatform(param.sound_thread_priority), sSoundThreadStackSize, attribute, param.enable_get_sound_thread_tick, heap);
    SEAD_ASSERT(result);

    // Initializes the SEQ modulation curve table.
    internal::CurveLfo::initializeCurveTable();

    //internal::AxVoice::setSamplesPerFrame(AX_IN_SAMPLES_PER_FRAME);
    //internal::AxVoice::setSamplesPerSec(AX_IN_SAMPLES_PER_SEC);

    //OboeInit(heap);
    InitSDK(heap);

    sIsInitialized = true;
}

void SoundSystem::finalize()
{
    if (!sIsInitialized)
        return;

    //OboeQuit();
    QuitSDK();

    // Ends the data load thread.
    internal::TaskMgr::instance()->cancelAllTask();
    internal::TaskThread::instance()->destroy();
    internal::TaskMgr::instance()->finalize();

    // Ends the sound thread.
    internal::driver::SoundThread::instance()->destroy();

    // Destroys the channel manager.
    internal::driver::ChannelMgr::instance()->finalize();
    internal::driver::MultiVoiceMgr::instance()->finalize();
    internal::driver::HardwareMgr::instance()->finalize();

    if (sIsInitializedVoiceRenderer)
    {
        internal::driver::SoundThread::instance()->detail_setVoiceRenderer(nullptr);

        sVoiceRenderer->finalize();
        delete sVoiceRenderer;
        sVoiceRenderer = nullptr;

        sSynthesizeBufferSize = 0;

        sIsInitializedVoiceRenderer = false;
    }

    // Finalizes the sound thread.
    internal::driver::SoundThread::instance()->finalize();

    SoundSystem::detail_finalizeDriverCommandMgr();

    sLoadThreadStackSize = 0;
    sSoundThreadStackSize = 0;

    sIsEnableVisualization = false;

    internal::TaskThread::deleteInstance();
    internal::driver::DisposeCallbackMgr::deleteInstance();
    internal::driver::ChannelMgr::deleteInstance();
    internal::driver::MultiVoiceMgr::deleteInstance();
    internal::TaskMgr::deleteInstance();
    internal::driver::SoundThread::deleteInstance();
    internal::driver::HardwareMgr::deleteInstance();

    sIsInitialized = false;
}

void SoundSystem::detail_initializeDriverCommandMgr(const SoundSystemParam& param, sead::Heap* heap)
{
    if (sIsInitializedDriverCommandMgr)
        return;

    internal::DriverCommand::createInstance(heap);
    internal::DriverCommandForTaskThread::createInstance(heap);

    internal::DriverCommand::instance()->initialize(param.sound_thread_command_buffer_size, heap);
    internal::DriverCommandForTaskThread::instance()->initialize(param.task_thread_command_buffer_size, heap);

    sSoundThreadCommandBufferSize = param.sound_thread_command_buffer_size;
    sTaskThreadCommandBufferSize = param.task_thread_command_buffer_size;

    sIsInitializedDriverCommandMgr = true;
}

void SoundSystem::detail_finalizeDriverCommandMgr()
{
    if (!sIsInitializedDriverCommandMgr)
        return;

    internal::DriverCommandForTaskThread::instance()->finalize();
    internal::DriverCommand::instance()->finalize();

    internal::DriverCommandForTaskThread::deleteInstance();
    internal::DriverCommand::deleteInstance();

    sSoundThreadCommandBufferSize = 0;
    sTaskThreadCommandBufferSize = 0;

    sIsInitializedDriverCommandMgr = false;
}

void SoundSystem::setSoundFrameUserCallback(SoundFrameUserCallback callback, void* arg)
{
    internal::driver::SoundThread::instance()->registerSoundFrameUserCallback(callback, arg);
}

void SoundSystem::clearSoundFrameUserCallback()
{
    internal::driver::SoundThread::instance()->clearSoundFrameUserCallback();
}

void SoundSystem::lockSoundThread()
{
    internal::driver::SoundThread::instance()->lock();
}

void SoundSystem::unlockSoundThread()
{
    internal::driver::SoundThread::instance()->unlock();
}

void SoundSystem::enterSleep()
{
    if (sIsEnterSleep)
        return;

    sIsEnterSleep = true;
    sIsStreamLoadWait = true;
    internal::TaskThread::instance()->lock();

    // The sound thread can be stopped on the SDK side.
}

void SoundSystem::leaveSleep()
{
    if (!sIsEnterSleep)
        return;

    // The sound thread recovers on the SDK side.
    internal::TaskThread::instance()->unlock();
    sIsStreamLoadWait = false;
    sIsEnterSleep = false;
}

f32 sVisualizationWaveData[SoundSystem::cSamplePerFrame] = { 0 };
f32 sWaveData[SoundSystem::cSamplePerFrame] = { 0 };
f32 sFFTData[SoundSystem::cSamplePerFrame] = { 0 };

f32* SoundSystem::getWave()
{
    {
        internal::driver::SoundThreadLock lock;

        for (u32 i = 0; i < cSamplePerFrame; i++)
        {
            sWaveData[i] = sVisualizationWaveData[i];
        }
    }

    return sWaveData;
}

f32* SoundSystem::calcFFT()
{
    const u32 bufSize = cSamplePerFrame + (2 - 1llu) & ~(2 - 1llu);

    f32 temp[bufSize * 4];

    {
        internal::driver::SoundThreadLock lock;

        for (u32 i = 0; i < bufSize; i++)
        {
            if (i < cSamplePerFrame)
                temp[i*2] = sVisualizationWaveData[i];
            else
                temp[i*2] = 0.0f;

            temp[i*2+1] = 0.0f;

            temp[i+bufSize*2] = 0.0f;
            temp[i+bufSize*3] = 0.0f;
        }
    }

    FFT::fft(temp, bufSize);

    for (u32 i = 0; i < cSamplePerFrame; i++)
    {
        f32 real = temp[i * 2];
        f32 imag = temp[i * 2 + 1];

        sFFTData[i] = sqrt(real*real+imag*imag);
    }

    return sFFTData;
}

static void DataCallback(ma_device* pDevice, void* pOutput, const void*, ma_uint32 frameCount)
{
    const u32 cChannelCount = internal::driver::HardwareMgr::cChannelCount;

    internal::driver::HardwareMgr::callHwCallback();
    //internal::driver::HardwareMgr::callHwCallback(); // do twice ???
    //internal::driver::HardwareMgr::callHwCallback(); // do thrice ???

    f32* dataBuffers[cChannelCount] = { internal::driver::HardwareMgr::sLeftDataBuffer, internal::driver::HardwareMgr::sRightDataBuffer };

    internal::driver::HardwareMgr::resetFinalMixCallbackData();
    internal::driver::HardwareMgr::processFinalMixCallback(dataBuffers, frameCount, cChannelCount);

    f32* outData = (f32*)pOutput;
    u32 idx = 0;

    for (u32 i = 0; i < frameCount * cChannelCount; i += cChannelCount)
    {
        const f32 cClampValue = 1.0f;

        f32 leftSample = internal::driver::HardwareMgr::sLeftDataBuffer[idx];
        leftSample = leftSample / 32767.0f;
        leftSample = sead::Mathf::clamp2(-cClampValue, leftSample, cClampValue);

        f32 rightSample = internal::driver::HardwareMgr::sRightDataBuffer[idx];
        rightSample = rightSample / 32767.0f;
        rightSample = sead::Mathf::clamp2(-cClampValue, rightSample, cClampValue);

        outData[i + 0] = leftSample;
        outData[i + 1] = rightSample;

        idx++;
    }
}

static ma_device device;

static void InitSDK(sead::Heap* heap)
{
    ma_device_config config    = ma_device_config_init(ma_device_type_playback);
    config.playback.format     = ma_format_f32;
    config.playback.channels   = internal::driver::HardwareMgr::cChannelCount;
    config.sampleRate          = internal::driver::HardwareMgr::cSampleRate;
    config.dataCallback        = DataCallback;
    config.pUserData           = nullptr;
    config.periodSizeInFrames  = internal::driver::HardwareMgr::cSamplePerFrame;

    config.noPreSilencedOutputBuffer = false;
    config.noClip = false;

    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
    {
        SEAD_ASSERT_MSG(false, "Failed to initialize the device.");
        return;
    }

    ma_device_start(&device); // The device is sleeping by default so you'll need to start it manually.
}

static void QuitSDK()
{
    ma_device_uninit(&device);
}

/*
class AudioDataCallback : public oboe::AudioStreamDataCallback
{
public:
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audioData, s32 numFrames) override
    {
        const u32 cChannelCount = 2;

        internal::driver::HardwareMgr::callHwCallback();
        internal::driver::HardwareMgr::callHwCallback(); // do twice ???
        internal::driver::HardwareMgr::callHwCallback(); // do thrice ???

        f32* dataBuffers[cChannelCount] = { internal::driver::HardwareMgr::sLeftDataBuffer, internal::driver::HardwareMgr::sRightDataBuffer };

        internal::driver::HardwareMgr::resetFinalMixCallbackData();
        internal::driver::HardwareMgr::processFinalMixCallback(dataBuffers, numFrames, cChannelCount);

        s16* outData = (s16*)audioData;
        u32 idx = 0;

        for (u32 i = 0; i < numFrames * cChannelCount; i += cChannelCount)
        {
            outData[i + 0] = internal::driver::HardwareMgr::sLeftDataBuffer[idx];
            outData[i + 1] = internal::driver::HardwareMgr::sRightDataBuffer[idx];

            idx++;
        }

        return oboe::DataCallbackResult::Continue;
    }
};

class AudioErrorCallback : public oboe::AudioStreamErrorCallback
{
public:
    void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override
    {
    }
};

AudioDataCallback sAudioDataCallback;
AudioErrorCallback sAudioErrorCallback;

oboe::AudioStream* sAudioStream = nullptr;

static void openStream(Heap* heap)
{
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output);
    builder.setAudioApi(oboe::AudioApi::AAudio);
    builder.setFormat(oboe::AudioFormat::I16);
    builder.setFormatConversionAllowed(true);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    builder.setUsage(oboe::Usage::Game);
    builder.setContentType(oboe::ContentType::Music);
    builder.setSampleRate(internal::driver::HardwareMgr::cSampleRate);
    builder.setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium);
    builder.setChannelCount(internal::driver::HardwareMgr::cChannelCount);
    builder.setFramesPerCallback(internal::driver::HardwareMgr::cSamplePerFrame);

    builder.setDataCallback(&sAudioDataCallback);
    builder.setErrorCallback(&sAudioErrorCallback);

    oboe::Result result;

    {
        ScopedCurrentHeap currentHeap(heap);

        result = builder.openStream(&sAudioStream);
    }

    if (result != oboe::Result::OK)
    {
        warn("Failed to open stream. Error: %s", oboe::convertToText(result));
        return;
    }

    SEAD_ASSERT(sAudioStream->getDirection() == oboe::Direction::Output);
    SEAD_ASSERT(sAudioStream->getAudioApi() == oboe::AudioApi::AAudio);
    SEAD_ASSERT(sAudioStream->getFormat() == oboe::AudioFormat::I16);
    SEAD_ASSERT(sAudioStream->getSampleRate() == internal::driver::HardwareMgr::cSampleRate);
    SEAD_ASSERT(sAudioStream->getChannelCount() == internal::driver::HardwareMgr::cChannelCount);
    SEAD_ASSERT(sAudioStream->getFramesPerCallback() == internal::driver::HardwareMgr::cSamplePerFrame);
}

static void OboeInit(Heap* heap)
{
    openStream(heap);

    if (!sAudioStream)
        return;

    oboe::Result result = sAudioStream->requestStart();
    if (result != oboe::Result::OK)
    {
        warn("Failed to start AudioStream");
        return;
    }

    //info("started AudioStream");
}

static void OboeQuit()
{
    if (!sAudioStream)
        return;

    sAudioStream->stop();
    sAudioStream->close();
    sAudioStream = nullptr;

    //info("closed AudioStream");
}
*/

} // namespace snd
