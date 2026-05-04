#pragma once

#include <basis/seadTypes.h>

#include "Channel.h"

#include "snd/snd_BankFileReader.h"
#include "snd/snd_WaveArchiveFileReader.h"
#include "snd/snd_WaveFileReader.h"

#include "snd/SequenceSoundPlayer.h"

#include <bfsar/BankFile.h>
#include <bfsar/WaveFile.h>

class BankR
{
public:
    static constexpr f32 VELOCITY_MAX_R = 1.0f / 127.0f;

    static constexpr f32 INSTRUMENT_VOLUME_CRITERION_R = 1.0f / 127.0f;

public:
    snd::internal::driver::Channel* noteOn(
        const nw::snd::internal::BankFileReader& bankReader,
        const nw::snd::internal::WaveArchiveFileReader& warcReader,
        const NoteOnInfo& noteOnInfo
    ) const
    {
        // Gets VelocityRegionInfo.
        nw::snd::internal::VelocityRegionInfo regionInfo;
        {
            if (!bankReader.ReadVelocityRegionInfo(
                    &regionInfo,
                    noteOnInfo.prgNo,
                    noteOnInfo.key,
                    noteOnInfo.velocity))
            {
                return nullptr;
            }
        }

        // Gets the waveform that corresponds to regnioInfo.
        // (The waveform archive corresponding to regionInfo.waveArchiveId is registered in warcReader and passed in an argument.)
        //  
        const void* waveFile = warcReader.GetWaveFile(regionInfo.waveIndex);
        SEAD_ASSERT(waveFile);

        // Gets the details of the waveform and allocates a channel.
        nw::snd::internal::WaveInfo waveInfo;
        {
            nw::snd::internal::WaveFileReader reader(waveFile);
            if (!reader.ReadWaveInfo(&waveInfo))
                return nullptr;
        }

        snd::internal::driver::Channel* pChannel = snd::internal::driver::Channel::allocChannel(
            sead::Mathi::min(static_cast<s32>(waveInfo.channelCount), 2),
            noteOnInfo.priority,
            noteOnInfo.channelCallback,
            noteOnInfo.channelCallbackData
        );

        if (!pChannel)
            return nullptr;

        // Initial parameter settings
        pChannel->setKey(static_cast<u8>(noteOnInfo.key), regionInfo.originalKey);
        pChannel->setVelocity(calcChannelVelocityVolume(static_cast<u8>(noteOnInfo.velocity)));
        pChannel->setInstrumentVolume(regionInfo.volume * INSTRUMENT_VOLUME_CRITERION_R);
        pChannel->setTune(regionInfo.pitch);

        pChannel->setAttack(regionInfo.adshrCurve.attack);
        pChannel->setHold(regionInfo.adshrCurve.hold);
        pChannel->setDecay(regionInfo.adshrCurve.decay);
        pChannel->setSustain(regionInfo.adshrCurve.sustain);
        pChannel->setRelease(regionInfo.adshrCurve.release);

        float initPan = static_cast<f32>(noteOnInfo.initPan + regionInfo.pan - 64) / 63.0f;
        pChannel->setInitPan(initPan);
        //pChannel->setInitSurroundPan(0.0f);

        pChannel->setKeyGroupId(regionInfo.keyGroup);
        pChannel->setIsIgnoreNoteOff(regionInfo.isIgnoreNoteOff);
        pChannel->setInterpolationType(regionInfo.interpolationType);
        pChannel->setUpdateType(noteOnInfo.updateType);

        snd::internal::WaveInfo waveInfoS;
        nw::snd::internal::ConvertWaveInfo(&waveInfoS, waveInfo);

        pChannel->start(waveInfoS, noteOnInfo.length, 0);

        return pChannel;
    }

    snd::internal::driver::Channel* noteOn(
        const BankFile& bankFile,
        const NoteOnInfo& noteOnInfo
    ) const
    {
        const BankFile::Instrument* instrument = bankFile.getInstrument(noteOnInfo.prgNo);
        if (!instrument)
        {
            return nullptr;
        }

        const BankFile::KeyRegion* keyRegion = instrument->getKeyRegion(noteOnInfo.key);
        if (!keyRegion)
        {
            return nullptr;
        }

        const BankFile::VelocityRegion* velocityRegion = keyRegion->getVelocityRegion(noteOnInfo.velocity);
        SEAD_ASSERT(velocityRegion);

        const WaveFile* waveFile = static_cast<const WaveFile*>(velocityRegion->getWaveFileRef().getItem());
        if (!waveFile || waveFile->getIsLoopDirty() || waveFile->getChannels().isEmpty())
        {
            return nullptr;
        }

        // Gets the details of the waveform and allocates a channel.
        nw::snd::internal::WaveInfo waveInfo;
        nw::snd::internal::GetWaveInfoFromWaveFile(&waveInfo, *waveFile);

        snd::internal::driver::Channel* pChannel = snd::internal::driver::Channel::allocChannel(
            sead::Mathi::min(static_cast<s32>(waveInfo.channelCount), 2),
            noteOnInfo.priority,
            noteOnInfo.channelCallback,
            noteOnInfo.channelCallbackData
        );

        if (!pChannel)
            return nullptr;

        // Initial parameter settings
        pChannel->setKey(static_cast<u8>(noteOnInfo.key), velocityRegion->getOriginalKey());
        pChannel->setVelocity(calcChannelVelocityVolume(static_cast<u8>(noteOnInfo.velocity)));
        pChannel->setInstrumentVolume(velocityRegion->getVolume() * INSTRUMENT_VOLUME_CRITERION_R);
        pChannel->setTune(velocityRegion->getPitch());

        pChannel->setAttack(velocityRegion->getAdshrCurve().attack);
        pChannel->setHold(velocityRegion->getAdshrCurve().hold);
        pChannel->setDecay(velocityRegion->getAdshrCurve().decay);
        pChannel->setSustain(velocityRegion->getAdshrCurve().sustain);
        pChannel->setRelease(velocityRegion->getAdshrCurve().release);

        float initPan = static_cast<f32>(noteOnInfo.initPan + velocityRegion->getPan() - 64) / 63.0f;
        pChannel->setInitPan(initPan);
        //pChannel->setInitSurroundPan(0.0f);

        pChannel->setKeyGroupId(velocityRegion->getKeyGroup());
        pChannel->setIsIgnoreNoteOff(velocityRegion->getIsIgnoreNoteOff());
        pChannel->setInterpolationType(velocityRegion->getInterpolationType());
        pChannel->setUpdateType(noteOnInfo.updateType);

        snd::internal::WaveInfo waveInfoS;
        nw::snd::internal::ConvertWaveInfo(&waveInfoS, waveInfo);

        pChannel->start(waveInfoS, noteOnInfo.length, 0);

        return pChannel;
    }

    static f32 calcChannelVelocityVolume(u8 velocity)
    {
        return static_cast<f32>(velocity) * VELOCITY_MAX_R;
    }
};
