#include "snd/HardwareMgr.h"

#include "snd/MultiVoiceMgr.h"
#include "snd/DriverCommand.h"
#include "snd/Macro.h"

#include <prim/seadScopedLock.h>
#include <prim/seadMemUtil.h>

namespace snd { namespace internal { namespace driver {

f32 HardwareMgr::sLeftDataBuffer[cSamplePerFrame] = { 0.0f };
f32 HardwareMgr::sRightDataBuffer[cSamplePerFrame] = { 0.0f };

const BiquadFilterLpf HardwareMgr::cBiquadFilterLpf;
const BiquadFilterHpf HardwareMgr::cBiquadFilterHpf;
const BiquadFilterBpf512 HardwareMgr::cBiquadFilterBpf512;
const BiquadFilterBpf1024 HardwareMgr::cBiquadFilterBpf1024;
const BiquadFilterBpf2048 HardwareMgr::cBiquadFilterBpf2048;

SEAD_SINGLETON_DISPOSER_IMPL(HardwareMgr);

HardwareMgr::HardwareMgr()
    : mIsInitialized(false)
    , mSrcType(SrcType::Type_4Tap)
{
    mFinalMixCallbackList.initOffset(offsetof(FinalMixCallback, mListNode));

    mMasterVolume.initValue(1.0f);
    mVolumeForReset.initValue(1.0f);

    mOutputMode = OutputMode::Num;
    mEndUserOutputMode = OutputMode::Num;
}

HardwareMgr::~HardwareMgr()
{
    mFinalMixCallbackList.clear();
}

void HardwareMgr::initialize()
{
    if (mIsInitialized)
        return;

    // Initialize the biquad filter table
    for (s32 i = (s32)BiquadFilterType::DataMin; i < (s32)BiquadFilterType::Max + 1; i++)
    {
        mBiquadFilterCallbackTable[i] = nullptr;
    }

    this->setBiquadFilterCallback(BiquadFilterType::Lpf, &cBiquadFilterLpf);
    this->setBiquadFilterCallback(BiquadFilterType::Hpf, &cBiquadFilterHpf);
    this->setBiquadFilterCallback(BiquadFilterType::Bpf512, &cBiquadFilterBpf512);
    this->setBiquadFilterCallback(BiquadFilterType::Bpf1024, &cBiquadFilterBpf1024);
    this->setBiquadFilterCallback(BiquadFilterType::Bpf2048, &cBiquadFilterBpf2048);

    // DeviceFinalMixCallback callback setting
    //HardwareMgr::registerDeviceFinalMixCallback(&HardwareMgr::FinalMixCallbackFunc);

    mEndUserOutputMode = OutputMode::Stereo;
    this->setOutputMode(OutputMode::Stereo);

    // Output device.
    for (s32 i = 0; i < (s32)OutputLineIdx::Max; i++)
    {
        mOutputDeviceFlag[i] = i < (s32)OutputLineIdx::ReservedMax ? 1 << i : 0;
    }

    mIsInitialized = true;
}

void HardwareMgr::finalize()
{
    if (!mIsInitialized)
        return;

    //HardwareMgr::registerDeviceFinalMixCallback(nullptr);

    mIsInitialized = false;
}

void HardwareMgr::update()
{
    // Update volume.
    if (!mMasterVolume.isFinished())
    {
        mMasterVolume.update();
        MultiVoiceMgr::instance()->updateAllVoicesSync(MultiVoice::cUpdateVe);
    }

    if (!mVolumeForReset.isFinished())
    {
        mVolumeForReset.update();
        MultiVoiceMgr::instance()->updateAllVoicesSync(MultiVoice::cUpdateVe);
    }
}

void HardwareMgr::setOutputMode(OutputMode mode)
{
    if (mOutputMode == mode)
        return;

    mOutputMode = mode;

    // Distributes commands.
    {
        DriverCommand* cmdMgr = DriverCommand::instance();

        DriverCommandAllVoicesSync* command = cmdMgr->allocCommand<DriverCommandAllVoicesSync>();
        command->id = DriverCommandId::AllVoicesSync;
        command->syncFlag = MultiVoice::cUpdateMix;

        cmdMgr->pushCommand(command);
    }
}

void HardwareMgr::updateEndUserOutputMode()
{
    mEndUserOutputMode = OutputMode::Stereo;
}

void HardwareMgr::setOutputDeviceFlag(u32 outputLineIndex, u8 outputDeviceFlag)
{
    if (outputLineIndex >= (u32)OutputLineIdx::ReservedMax && outputLineIndex < (u32)OutputLineIdx::Max)
    {
        if (mOutputDeviceFlag[outputLineIndex] == outputDeviceFlag)
            return;

        mOutputDeviceFlag[outputLineIndex] = outputDeviceFlag;

        // Distributes commands.
        {
            DriverCommand* cmdMgr = DriverCommand::instance();

            DriverCommandAllVoicesSync* command = cmdMgr->allocCommand<DriverCommandAllVoicesSync>();
            command->id = DriverCommandId::AllVoicesSync;
            command->syncFlag = MultiVoice::cUpdateMix;

            cmdMgr->pushCommand(command);
        }
    }
}

f32 HardwareMgr::getOutputVolume() const
{
    f32 volume = mMasterVolume.getValue();
    volume *= mVolumeForReset.getValue();

    return volume;
}

void HardwareMgr::setMasterVolume(f32 volume, s32 fadeTimes)
{
    if (volume < 0.0f)
        volume = 0.0f;

    mMasterVolume.setTarget(volume, (fadeTimes + cSoundFrameIntervalMSEC - 1) / cSoundFrameIntervalMSEC);

    if (fadeTimes == 0)
    {
        // Distributes commands.
        DriverCommand* cmdMgr = DriverCommand::instance();

        DriverCommandAllVoicesSync* command = cmdMgr->allocCommand<DriverCommandAllVoicesSync>();
        command->id = DriverCommandId::AllVoicesSync;
        command->syncFlag = MultiVoice::cUpdateVe;

        cmdMgr->pushCommand(command);
    }
}

void HardwareMgr::setSrcType(SrcType type)
{
    if (mSrcType == type)
        return;

    mSrcType = type;

    // Distributes commands.
    {
        DriverCommand* cmdMgr = DriverCommand::instance();

        DriverCommandAllVoicesSync* command = cmdMgr->allocCommand<DriverCommandAllVoicesSync>();
        command->id = DriverCommandId::AllVoicesSync;
        command->syncFlag = MultiVoice::cUpdateSrc;

        cmdMgr->pushCommand(command);
    }
}

void HardwareMgr::setBiquadFilterCallback(BiquadFilterType type, const BiquadFilterCallback* cb)
{
    SEAD_ASSERT((s32)type >= (s32)BiquadFilterType::DataMin && (s32)type <= (s32)BiquadFilterType::Max);

    if ((s32)type <= (s32)BiquadFilterType::None /* 0 */)
    {
        // Does nothing because 0 indicates the Biquad filter is off.
        return;
    }

    mBiquadFilterCallbackTable[(s32)type] = cb;
}

void HardwareMgr::prepareReset()
{
    mVolumeForReset.setTarget(0.0f, 3); // Fade out over 3 audio frames.
}

bool HardwareMgr::isResetReady() const
{
    return true;
}

void HardwareMgr::appendFinalMixCallback(FinalMixCallback* callback)
{
    SEAD_ASSERT(callback);

    sead::ScopedLock<sead::CriticalSection> lock(&mCriticalSection);
    mFinalMixCallbackList.pushBack(callback);
}

void HardwareMgr::prependFinalMixCallback(FinalMixCallback* callback)
{
    SEAD_ASSERT(callback);

    sead::ScopedLock<sead::CriticalSection> lock(&mCriticalSection);
    mFinalMixCallbackList.pushFront(callback);
}

void HardwareMgr::eraseFinalMixCallback(FinalMixCallback* callback)
{
    SEAD_ASSERT(callback);

    sead::ScopedLock<sead::CriticalSection> lock(&mCriticalSection);
    mFinalMixCallbackList.erase(callback);
}

void HardwareMgr::onFinalMixCallback(f32** data, u32 sampleCount, u32 channelCount)
{
    SEAD_ASSERT(data);

    // bad
    //ScopedLock<CriticalSection> lock(mCriticalSection);

    FinalMixData mixData;
    if (channelCount > 0)
    {
        if (channelCount > 0)
            mixData.left = data[0];
        else
            mixData.left = nullptr;

        if (channelCount > 1)
            mixData.right = data[1];
        else
            mixData.right = nullptr;
    }

    mixData.sampleCount = sampleCount;
    mixData.channelCount = channelCount;

    for (auto& cb : mFinalMixCallbackList)
    {
        cb.onFinalMix(&mixData);
    }
}

//static SafeArray<HardwareMgr::HardwareCallback, 64> sHwCallback = { nullptr };
static HardwareMgr::HardwareCallback sHwCallback = nullptr;

void HardwareMgr::registerHwCallback(HardwareCallback callback)
{
    sHwCallback = callback;
}

void HardwareMgr::deregisterHwCallback(HardwareCallback callback)
{
    sHwCallback = nullptr;
}

void HardwareMgr::callHwCallback()
{
    if (sHwCallback)
        sHwCallback();
}

void HardwareMgr::resetFinalMixCallbackData()
{
    sead::MemUtil::fillZero(sLeftDataBuffer, cSamplePerFrame * sizeof(f32));
    sead::MemUtil::fillZero(sRightDataBuffer, cSamplePerFrame * sizeof(f32));
}

void HardwareMgr::processFinalMixCallback(f32** data, u32 sampleCount, u32 channelCount)
{
    HardwareMgr::instance()->onFinalMixCallback(data, sampleCount, channelCount);
}

} } } // namespace snd::internal::driver
