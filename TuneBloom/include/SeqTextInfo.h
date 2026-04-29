#pragma once

#include <snd/SequenceSoundPlayer.h>

#include <TextEditor.h>

#include <unordered_map>

#include <container/seadRingBuffer.h>

class SeqTextInfo
{
public:
    struct Track
    {
        Track()
            : active(false), updated(false), offset(0), line(0)
        {
        }

        bool active;
        bool updated;
        u32 offset;
        s32 line;

        struct HistoryLine
        {
            HistoryLine()
                : life(0), offset(0), line(0)
            {
            }

            static const u32 cMaxLife;

            u32 life;
            u32 offset;
            s32 line;
        };

        sead::FixedRingBuffer<HistoryLine, 64> historyLines;
    };

public:
    SeqTextInfo()
    {
        reset();
    }

    void reset();
    void update(const SequenceSoundPlayer& player, TextEditor& editor, std::unordered_map<u32, u32>& offsetToLine, bool showTrail);

    void pushHistory(u32 track, u32 offset);

    Track mTracks[SequenceSoundPlayer::cTrackNumPerPlayer];
};
