#pragma once

#include <snd/SequenceSoundPlayer.h>

#include <TextEditor.h>

#include <unordered_map>

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
    };

public:
    SeqTextInfo()
    {
        reset();
    }

    void reset()
    {
        for (u32 i = 0; i < SequenceSoundPlayer::cTrackNumPerPlayer; i++)
        {
            mTracks[i].active = false;
        }
    }

    void update(const SequenceSoundPlayer& player, TextEditor& editor, std::unordered_map<u32, u32>& offsetToLine)
    {
        for (u32 i = 0; i < SequenceSoundPlayer::cTrackNumPerPlayer; i++)
        {
            const SequenceTrack* track = player.getPlayerTrack(i);
            if (track)
            {
                mTracks[i].active = true;
            }

            if (!mTracks[i].active)
            {
                continue;
            }

            track = &player.getTrack_(i);

            mTracks[i].offset = intptr_t(track->getParserTrackParam().currentCmdAddr) - intptr_t(track->getParserTrackParam().baseAddr);

            u32 line = offsetToLine[mTracks[i].offset];
            mTracks[i].updated = line != mTracks[i].line;
            mTracks[i].line = line;
        }

        TextEditor::Markers markers;
        for (u32 i = 0; i < SequenceSoundPlayer::cTrackNumPerPlayer; i++)
        {
            static const ImU32 sTrackColors[SequenceSoundPlayer::cTrackNumPerPlayer] = {
                IM_COL32(7,   83, 175, 0.8f * 255),
                IM_COL32(67,  78, 184, 0.8f * 255),
                IM_COL32(88,  71, 190, 0.8f * 255),
                IM_COL32(103, 65, 195, 0.8f * 255),
                IM_COL32(116, 58, 201, 0.8f * 255),
                IM_COL32(125, 51, 205, 0.8f * 255),
                IM_COL32(143, 37, 214, 0.8f * 255),
                IM_COL32(153, 23, 219, 0.8f * 255),
                IM_COL32(166, 7,  221, 0.8f * 255),
                IM_COL32(180, 14, 210, 0.8f * 255),
                IM_COL32(189, 18, 197, 0.8f * 255),
                IM_COL32(197, 21, 188, 0.8f * 255),
                IM_COL32(206, 24, 179, 0.8f * 255),
                IM_COL32(214, 25, 168, 0.8f * 255),
                IM_COL32(230, 28, 142, 0.8f * 255),
                IM_COL32(246, 31, 111, 0.8f * 255),
            };

            const Track& track = mTracks[i];
            if (track.active)
            {
                auto it = markers.find(track.line);
                if (it == markers.end())
                {
                    markers[track.line] = { std::string("Track ") + std::to_string(i), sTrackColors[i], false, true };
                }
                else
                {
                    it->second.mDetail += std::string(", Track ") + std::to_string(i);
                }
            }
        }

        editor.SetMarkers(markers);
    }

    Track mTracks[SequenceSoundPlayer::cTrackNumPerPlayer];
};
