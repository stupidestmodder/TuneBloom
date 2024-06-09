#include "SequenceSoundPlayer.h"

#include "BankR.h"

#include <bfsar/BankFile.h>

#include <basis/seadWarning.h>

class SequenceNoteOnCallback : public NoteOnCallback
{
public:
    SequenceNoteOnCallback()
    {
    }

    snd::internal::driver::Channel* noteOn(SequenceSoundPlayer* seqPlayer, u8 bankIndex, const NoteOnInfo& noteOnInfo) override
    {
        const nw::snd::internal::BankFileReader& bankReader = seqPlayer->getBankFileReader(bankIndex);

        if (!bankReader.IsInitialized())
        {
            SEAD_WARNING("invalid bank index(%d) is used in SequenceSound.", bankIndex);
        }

        // Gets the waveform archive ID and the index inside of the waveform archive.
        BankR bank;
        snd::internal::driver::Channel* channel = bank.noteOn(
            bankReader,
            seqPlayer->getWaveArchiveFileReader(bankIndex),
            noteOnInfo
        );

        return channel;
    }
};

class SequenceNoteOnCallback2 : public NoteOnCallback
{
public:
    SequenceNoteOnCallback2()
    {
    }

    snd::internal::driver::Channel* noteOn(SequenceSoundPlayer* seqPlayer, u8 bankIndex, const NoteOnInfo& noteOnInfo) override
    {
        const BankFile* bankFile = seqPlayer->getBankFile(bankIndex);

        if (!bankFile)
        {
            SEAD_WARNING("invalid bank index(%d) is used in SequenceSound.", bankIndex);
            return nullptr;
        }

        BankR bank;
        snd::internal::driver::Channel* channel = bank.noteOn(
            *bankFile,
            noteOnInfo
        );

        return channel;
    }
};
