#include "snd/MmlParser.h"

#include "snd/MmlCommand.h"
#include "snd/SequenceSoundPlayer.h"

#include <basis/seadWarning.h>
#include <math/seadMathCalcCommon.h>
#include <random/seadGlobalRandom.h>

const f32 MOD_SPEED_BASE = 0.390625f;                   // 6.25Hz because it is x 16.

SequenceTrack::ParseResult MmlParser::parse(SequenceTrack* track, bool doNoteOn) const
{
    SEAD_ASSERT(track);

    SequenceSoundPlayer* player = track->getSequenceSoundPlayer();
    SEAD_ASSERT(player);

    SequenceTrack::ParserTrackParam& trackParam = track->getParserTrackParam();

    SeqArgType argType = SEQ_ARG_NONE;
    SeqArgType argType2 = SEQ_ARG_NONE;
    bool useArgType = false;
    bool doExecCommand = true;

    trackParam.currentCmdAddr = trackParam.currentAddr; // Custom

    u32 cmd = ReadByte(&trackParam.currentAddr);

    // processing for the prefix binary
    if (cmd == MmlCommand::MML_IF)
    {
        cmd = ReadByte(&trackParam.currentAddr);
        doExecCommand = trackParam.cmpFlag != 0;
    }

    // second parameter
    if (cmd == MmlCommand::MML_TIME)
    {
        cmd = ReadByte(&trackParam.currentAddr);
        argType2 = SEQ_ARG_S16;
    }
    else if (cmd == MmlCommand::MML_TIME_RANDOM)
    {
        cmd = ReadByte(&trackParam.currentAddr);
        argType2 = SEQ_ARG_RANDOM;
    }
    else if (cmd == MmlCommand::MML_TIME_VARIABLE)
    {
        cmd = ReadByte(&trackParam.currentAddr);
        argType2 = SEQ_ARG_VARIABLE;
    }

    // Parameters.
    if (cmd == MmlCommand::MML_RANDOM)
    {
        cmd = ReadByte(&trackParam.currentAddr);
        argType = SEQ_ARG_RANDOM;
        useArgType = true;
    }
    else if (cmd == MmlCommand::MML_VARIABLE)
    {
        cmd = ReadByte(&trackParam.currentAddr);
        argType = SEQ_ARG_VARIABLE;
        useArgType = true;
    }

    if ((cmd & 0x80) == 0)
    {
        // 'Note' command
        const u8 velocity = ReadByte(&trackParam.currentAddr);
        const s32 length = ReadArg(
            &trackParam.currentAddr,
            player,
            track,
            useArgType ? argType : SEQ_ARG_VMIDI
        );

        int key = static_cast<int>(cmd) + trackParam.transpose;

        if (!doExecCommand)
            return SequenceTrack::PARSE_RESULT_CONTINUE;

        key = sead::Mathi::clamp2(0, key, 127);

        if (! trackParam.muteFlag && doNoteOn)
        {
            noteOnCommandProc(
                track,
                key,
                velocity,
                length > 0 ? length : -1,
                trackParam.tieFlag
          );
        }

        if (trackParam.noteWaitFlag) {
            trackParam.wait = length;
            trackParam.waitAmount = length; // Custom
            if (length == 0) {
                trackParam.noteFinishWait = true;
            }
        }
    }
    else
    {
        s32 commandArg1 = 0;
        s32 commandArg2 = 0;

        switch (cmd & 0xf0)
        {
        case 0x80:
        {
            switch (cmd)
            {
            case MmlCommand::MML_WAIT:
            {
                // Process here.
                s32 arg = ReadArg(
                    &trackParam.currentAddr,
                    player,
                    track,
                    useArgType ? argType : SEQ_ARG_VMIDI
              );
                if (doExecCommand)
                {
                    trackParam.wait = arg;
                    trackParam.waitAmount = arg; // Custom
                }
                break;
            }
            case MmlCommand::MML_PRG:
            {
                commandArg1 = ReadArg(
                    &trackParam.currentAddr,
                    player,
                    track,
                    useArgType ? argType : SEQ_ARG_VMIDI
              );
                if (doExecCommand)
                {
                    commandProc(
                        track,
                        cmd,
                        commandArg1,
                        commandArg2
                  );
                }
                break;
            }
            case MmlCommand::MML_OPEN_TRACK:
            {
                u8 trackNo = ReadByte(&trackParam.currentAddr);
                u32 offset = Read24(&trackParam.currentAddr);
                if (doExecCommand)
                {
                    commandArg1 = trackNo;
                    commandArg2 = static_cast<signed long>(offset);
                    commandProc(
                        track,
                        cmd,
                        commandArg1,
                        commandArg2
                  );
                }
                break;
            }
            case MmlCommand::MML_JUMP:
            {
                u32 offset = Read24(&trackParam.currentAddr);
                if (doExecCommand)
                {
                    commandArg1 = static_cast<signed long>(offset);
                    commandProc(
                        track,
                        cmd,
                        commandArg1,
                        commandArg2
                  );
                }
                break;
            }
            case MmlCommand::MML_CALL:
            {
                u32 offset = Read24(&trackParam.currentAddr);
                if (doExecCommand)
                {
                    commandArg1 = static_cast<signed long>(offset);
                    commandProc(
                        track,
                        cmd,
                        commandArg1,
                        commandArg2
                  );
                }
                break;
            }
            }
            break; // case 0x88 - 0x8f
        }

        case 0xb0: case 0xc0: case 0xd0: // u8 parameters (mixed in places with s8 parameters)
        {
            u8 arg = static_cast<u8>(ReadArg(
                &trackParam.currentAddr,
                player,
                track,
                useArgType ? argType : SEQ_ARG_U8
          ));

            if (argType2 != SEQ_ARG_NONE)
            {
                commandArg2 = ReadArg(
                    &trackParam.currentAddr,
                    player,
                    track,
                    argType2
              );
            }

            if (doExecCommand)
            {
                switch (cmd)
                {
                case MmlCommand::MML_TRANSPOSE:
                case MmlCommand::MML_PITCH_BEND:
                    commandArg1 = *reinterpret_cast<s8*>(&arg);
                    break;
                default:
                    commandArg1 = arg;
                }

                commandProc(
                    track,
                    cmd,
                    commandArg1,
                    commandArg2
              );
            }
            break;
        }

        case 0x90: // Extra
        {
            if (doExecCommand)
            {
                commandProc(
                    track,
                    cmd,
                    commandArg1,
                    commandArg2
              );
            }
            break;
        }

        case 0xe0: // s16 parameters
        {
            commandArg1 = static_cast<s16>(ReadArg(
                &trackParam.currentAddr,
                player,
                track,
                useArgType ? argType : SEQ_ARG_S16
          ));

            if (doExecCommand)
            {
                commandProc(
                    track,
                    cmd,
                    commandArg1,
                    commandArg2
              );
            }
            break;
        }

        case 0xf0: // either no parameters, or extension command
        {

            switch (cmd)
            {
            case MmlCommand::MML_ALLOC_TRACK:
                (void)Read16(&trackParam.currentAddr);
                //SEAD_ASSERT_MSG(false, "seq: must use alloctrack in startup code");
                //break;
                SEAD_WARNING("seq: must use alloctrack in startup code");
                return SequenceTrack::PARSE_RESULT_FINISH;

            case MmlCommand::MML_FIN:
                if (doExecCommand)
                {
                    return SequenceTrack::PARSE_RESULT_FINISH;
                }
                break;

            case MmlCommand::MML_EX_COMMAND:
            {
                u32 cmdex = ReadByte(&trackParam.currentAddr);

                switch (cmdex & 0xf0)
                {
                case 0xa0: case 0xb0: // u8 parameters.
                {
                    commandArg1 = ReadByte(&trackParam.currentAddr);
                    if (doExecCommand)
                    {
                        commandProc(
                            track,
                            (cmd << 8) + cmdex,
                            commandArg1,
                            commandArg2
                      );
                    }
                    break;
                }

                case 0xe0: // u16 parameter
                {
                    commandArg1 = static_cast<u16>(ReadArg(
                        &trackParam.currentAddr,
                        player,
                        track,
                        useArgType ? argType : SEQ_ARG_S16
                  ));
                    if (doExecCommand)
                    {
                        commandProc(
                            track,
                            (cmd << 8) + cmdex,
                            commandArg1,
                            commandArg2
                      );
                    }
                    break;
                }

                case 0x80: case 0x90: // 2 parameters
                {
                    commandArg1 = ReadByte(&trackParam.currentAddr);
                    commandArg2 = static_cast<s16>(ReadArg(
                        &trackParam.currentAddr,
                        player,
                        track,
                        useArgType ? argType : SEQ_ARG_S16
                  ));
                    if (doExecCommand)
                    {
                        commandProc(
                            track,
                            (cmd << 8) + cmdex,
                            commandArg1,
                            commandArg2
                      );
                    }
                    break;
                }
                }
                break;
            } // case MmlCommand::MML_EX_COMMAND

            default:
            {
                if (doExecCommand)
                {
                    commandProc(
                        track,
                        cmd,
                        commandArg1,
                        commandArg2
                  );
                }
                break;
            }
            } // case 0xf0 - 0xff
            break;
        } // case 0xf0

        case 0xa0: // prefix commands
        {
            // Having a prefix command here is invalid.
            SEAD_ASSERT_MSG(false, "Invalid seqdata command: %d", cmd);
        }
        }
    }

    return SequenceTrack::PARSE_RESULT_CONTINUE;
}

void MmlParser::commandProc(SequenceTrack* track, u32 command, s32 commandArg1, s32 commandArg2) const
{
    SEAD_ASSERT(track);

    SequenceSoundPlayer* player = track->getSequenceSoundPlayer();
    SEAD_ASSERT(player);

    SequenceTrack::ParserTrackParam& trackParam = track->getParserTrackParam();
    SequenceSoundPlayer::ParserPlayerParam& playerParam = player->getParserPlayerParam();

    if (command <= 0xff) // 1 byte command
    {
        switch (command)
        {

        case MmlCommand::MML_TEMPO:
            playerParam.tempo = static_cast<u16>(
                sead::Mathi::clamp2(TEMPO_MIN, static_cast<s32>(commandArg1), TEMPO_MAX)
           );
            break;

        case MmlCommand::MML_TIMEBASE:
            playerParam.timebase = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_PRG:
            if (commandArg1 < 0x10000)
            {
                trackParam.prgNo = static_cast<u16>(commandArg1);
            }
            else
            {
                SEAD_WARNING("nw::snd::MmlParser: too large prg No. %d", commandArg1);
            }
            break;

        case MmlCommand::MML_MUTE:
            //track->SetMute(static_cast<SeqMute>(commandArg1));
            break;

        case MmlCommand::MML_VOLUME:
            trackParam.volume.setTarget(
                static_cast<u8>(commandArg1),
                static_cast<s16>(commandArg2)
           );
            break;

        case MmlCommand::MML_VOLUME2:
            trackParam.volume2.setTarget(
                static_cast<u8>(commandArg1),
                static_cast<s16>(commandArg2)
           );
            break;

        case MmlCommand::MML_VELOCITY_RANGE:
            trackParam.velocityRange = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MAIN_VOLUME:
            playerParam.volume.setTarget(
                static_cast<u8>(commandArg1),
                static_cast<s16>(commandArg2)
           );
            break;

        case MmlCommand::MML_TRANSPOSE:
            trackParam.transpose = static_cast<s8>(commandArg1);
            break;

        case MmlCommand::MML_PITCH_BEND:
            trackParam.pitchBend.setTarget(
                static_cast<s8>(commandArg1),
                static_cast<s16>(commandArg2)
           );
            break;

        case MmlCommand::MML_BEND_RANGE:
            trackParam.bendRange = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_PAN:
            trackParam.pan.setTarget(
                static_cast<s8>(commandArg1 - PAN_CENTER),
                static_cast<s16>(commandArg2)
           );
            break;

        case MmlCommand::MML_INIT_PAN:
            trackParam.initPan = static_cast<s8>(commandArg1 - PAN_CENTER);
            break;

        case MmlCommand::MML_SURROUND_PAN:
            trackParam.surroundPan.setTarget(
                static_cast<s8>(commandArg1),
                static_cast<s16>(commandArg2)
           );
            break;

        case MmlCommand::MML_PRIO:
            trackParam.priority = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_NOTE_WAIT:
            trackParam.noteWaitFlag = (commandArg1 != 0);
            break;

        case MmlCommand::MML_FRONT_BYPASS:
            trackParam.frontBypassFlag = (commandArg1 != 0);
            break;

        case MmlCommand::MML_PORTA_TIME:
            trackParam.portaTime = static_cast<u8>(commandArg1);
            break;

        // Modulation 1 {{
        case MmlCommand::MML_MOD_DEPTH:
            trackParam.lfoParam[0].depth = static_cast<u8>(commandArg1) / 128.0f;
            break;

        case MmlCommand::MML_MOD_SPEED:
            trackParam.lfoParam[0].speed = static_cast<u8>(commandArg1) * MOD_SPEED_BASE;
            break;

        case MmlCommand::MML_MOD_PERIOD:
            {
                s16 arg = static_cast<s16>(commandArg1);
                if (arg == 0)
                {
                    trackParam.lfoParam[0].speed = 0.0f;
                }
                else
                {
                    trackParam.lfoParam[0].speed = 100.f / arg;
                }
            }
            break;

        case MmlCommand::MML_MOD_PHASE:
            trackParam.lfoParam[0].phase = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MOD_TYPE:
            trackParam.lfoTarget[0] = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MOD_RANGE:
            trackParam.lfoParam[0].range = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MOD_DELAY:
            trackParam.lfoParam[0].delay = static_cast<u32>(commandArg1 * 5); // in 5ms units
            break;

        case MmlCommand::MML_MOD_CURVE:
            trackParam.lfoParam[0].curve = static_cast<u8>(commandArg1);
            break;
        // }}


        case MmlCommand::MML_SWEEP_PITCH:
            trackParam.sweepPitch = static_cast<f32>(commandArg1) / 64.0f;
            break;

        case MmlCommand::MML_ATTACK:
            trackParam.attack = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_DECAY:
            trackParam.decay = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_SUSTAIN:
            trackParam.sustain = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_RELEASE:
            trackParam.release = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_ENV_HOLD:
            trackParam.envHold = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_ENV_RESET:
            trackParam.attack = SequenceTrack::cInvalidEnvelope;
            trackParam.decay = SequenceTrack::cInvalidEnvelope;
            trackParam.sustain = SequenceTrack::cInvalidEnvelope;
            trackParam.release = SequenceTrack::cInvalidEnvelope;
            trackParam.envHold = SequenceTrack::cInvalidEnvelope;
            break;

        case MmlCommand::MML_DAMPER:
            trackParam.damperFlag = (static_cast<u8>(commandArg1) >= 64);
            break;

        case MmlCommand::MML_TIE:
            trackParam.tieFlag = (commandArg1 != 0);
            track->releaseAllChannel(-1);
            track->freeAllChannel();
            break;

        case MmlCommand::MML_MONOPHONIC:
            trackParam.monophonicFlag = (commandArg1 != 0);
            if (trackParam.monophonicFlag)
            {
                track->releaseAllChannel(-1);
                track->freeAllChannel();
            }
            break;

        case MmlCommand::MML_PORTA:
            trackParam.portaKey = static_cast<u8>(commandArg1 + trackParam.transpose);
            trackParam.portaFlag = true;
            break;

        case MmlCommand::MML_PORTA_SW:
            trackParam.portaFlag = (commandArg1 != 0);
            break;

        case MmlCommand::MML_LPF_CUTOFF:
            trackParam.lpfFreq = static_cast<f32>(commandArg1 - 64) / 64.0f;
            break;

        case MmlCommand::MML_BIQUAD_TYPE:
            trackParam.biquadType = static_cast<s8>(commandArg1);
            break;

        case MmlCommand::MML_BIQUAD_VALUE:
            trackParam.biquadValue = static_cast<f32>(commandArg1) / 127.0f;
            break;

        case MmlCommand::MML_BANK_SELECT:
            trackParam.bankIndex = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_FXSEND_A:
            trackParam.fxSend[ (u32)snd::AuxBus::A ] = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_FXSEND_B:
            trackParam.fxSend[ (u32)snd::AuxBus::B ] = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_FXSEND_C:
            trackParam.fxSend[ (u32)snd::AuxBus::C ] = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MAINSEND:
            trackParam.mainSend = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_PRINTVAR:
            // TODO: Why is this commented ?
            // if (mPrintVarEnabledFlag) {
            //     const vs16* const varPtr = GetVariablePtr(player, track, commandArg1);
            //     NW_NULL_ASSERT(varPtr);
            //     NW_LOG("#%08x[%d]: printvar %sVAR_%d(%d) = %d\n",
            //         player,
            //         track->GetPlayerTrackNo(),
            //         (commandArg1 >= 32)? "T": (commandArg1 >= 16)? "G": "",
            //         (commandArg1 >= 32)? commandArg1-32: (commandArg1 >= 16)? commandArg1-16: commandArg1,
            //         commandArg1,
            //         *varPtr);
            // }
            break;

        case MmlCommand::MML_OPEN_TRACK:
        {
            SequenceTrack* newTrack = player->getPlayerTrack(commandArg1);
            if (newTrack == NULL)
            {
                SEAD_WARNING("nw::snd::MmlParser: opentrack for not allocated track");
                break;
            }
            if (newTrack == track)
            {
                SEAD_WARNING("nw::snd::MmlParser: opentrack for self track");
                break;
            }
            newTrack->close();
            newTrack->setSeqData(trackParam.baseAddr, commandArg2);
            newTrack->open();
            break;
        }

        case MmlCommand::MML_JUMP:
            trackParam.currentAddr = trackParam.baseAddr + commandArg1;
            break;

        case MmlCommand::MML_CALL:
        {
            if (trackParam.callStackDepth >= SequenceTrack::cCallStackDepth) {
                SEAD_WARNING("nw::snd::MmlParser: cannot 'call' because already too deep");
                break;
            }

            SequenceTrack::ParserTrackParam::CallStack* callStack = &trackParam.callStack[ trackParam.callStackDepth ];
            callStack->address = trackParam.currentAddr;
            callStack->loopFlag = false;
            trackParam.callStackDepth++;
            trackParam.currentAddr = trackParam.baseAddr + commandArg1;
            break;
        }

        case MmlCommand::MML_RET:
        {
            SequenceTrack::ParserTrackParam::CallStack* callStack = NULL;
            while(trackParam.callStackDepth > 0) {
                trackParam.callStackDepth--;
                if (! trackParam.callStack[ trackParam.callStackDepth ].loopFlag) {
                    callStack = &trackParam.callStack[ trackParam.callStackDepth ];
                    break;
                }
            }
            if (callStack == NULL) {
                SEAD_WARNING("nw::snd::MmlParser: unmatched sequence command 'ret'");
                break;
            }
            trackParam.currentAddr = callStack->address;
            break;
        }

        case MmlCommand::MML_LOOP_START:
        {
            if (trackParam.callStackDepth >= SequenceTrack::cCallStackDepth) {
                SEAD_WARNING("nw::snd::MmlParser: cannot 'loop_start' because already too deep");
                break;
            }

            SequenceTrack::ParserTrackParam::CallStack* callStack = &trackParam.callStack[ trackParam.callStackDepth ];
            callStack->address = trackParam.currentAddr;
            callStack->loopCount = static_cast<u8>(commandArg1);
            callStack->loopFlag = true;
            trackParam.callStackDepth++;
            break;
        }

        case MmlCommand::MML_LOOP_END:
        {
            if (trackParam.callStackDepth == 0) {
                SEAD_WARNING("nw::snd::MmlParser: unmatched sequence command 'loop_end'");
                break;
            }

            SequenceTrack::ParserTrackParam::CallStack* callStack = & trackParam.callStack[ trackParam.callStackDepth - 1 ];
            if (! callStack->loopFlag) {
                SEAD_WARNING("nw::snd::MmlParser: unmatched sequence command 'loop_end'");
                break;
            }

            u8 loop_count = callStack->loopCount;
            if (loop_count > 0) {
                loop_count--;
                if (loop_count == 0) {
                    trackParam.callStackDepth--;
                    break;
                }
            }

            callStack->loopCount = loop_count;
            trackParam.currentAddr = callStack->address;
            break;
        }
        }
    }
    else if (command <= 0xffff) // 2 byte command
    {
    #ifdef NW_CONSOLE_ENABLE
        u32 cmd = command >> 8;
        SEAD_ASSERT(cmd == MmlCommand::MML_EX_COMMAND);
    #endif // NW_CONSOLE_ENABLE
        u32 cmdex = command & 0xff;

        // -----------------------------------------------------------------
        // Extended command

        volatile s16* varPtr = NULL;
        if (((cmdex & 0xf0) == 0x80) ||
             ((cmdex & 0xf0) == 0x90)
          )
        {
            // For sequence variable commands, get pointer to the sequence variable
            varPtr = GetVariablePtr(player, track, commandArg1);
            if (varPtr == NULL) return;
        }


        switch (cmdex)
        {
        case MmlCommand::MML_SETVAR:
            *varPtr = static_cast<s16>(commandArg2);
            break;

        case MmlCommand::MML_ADDVAR:
            *varPtr += static_cast<s16>(commandArg2);
            break;

        case MmlCommand::MML_SUBVAR:
            *varPtr -= static_cast<s16>(commandArg2);
            break;

        case MmlCommand::MML_MULVAR:
            *varPtr *= static_cast<s16>(commandArg2);
            break;

        case MmlCommand::MML_DIVVAR:
            if (commandArg2 != 0) *varPtr /= static_cast<s16>(commandArg2);
            break;

        case MmlCommand::MML_SHIFTVAR:
            if (commandArg2 >= 0)
            {
                *varPtr <<= commandArg2;
            }
            else
            {
                *varPtr >>= -commandArg2;
            }
            break;

        case MmlCommand::MML_RANDVAR:
        {
            bool minus_flag = false;
            s32 rand;

            if (commandArg2 < 0) {
                minus_flag = true;
                commandArg2 = static_cast<s16>(-commandArg2);
            }

            rand = sead::GlobalRandom::instance()->getU32(0xFFFF);
            rand *= commandArg2 + 1;
            rand >>= 16;
            if (minus_flag) rand = -rand;
            *varPtr = static_cast<s16>(rand);
            break;

        }

        case MmlCommand::MML_ANDVAR:
            *varPtr &= commandArg2;
            break;

        case MmlCommand::MML_ORVAR:
            *varPtr |= commandArg2;
            break;

        case MmlCommand::MML_XORVAR:
            *varPtr ^= commandArg2;
            break;

        case MmlCommand::MML_NOTVAR:
            *varPtr = static_cast<s16>(~static_cast<u16>(commandArg2));
            break;

        case MmlCommand::MML_MODVAR:
            if (commandArg2 != 0) *varPtr %= commandArg2;
            break;

        case MmlCommand::MML_CMP_EQ:
            trackParam.cmpFlag = (*varPtr == commandArg2) ;
            break;

        case MmlCommand::MML_CMP_GE:
            trackParam.cmpFlag = (*varPtr >= commandArg2) ;
            break;

        case MmlCommand::MML_CMP_GT:
            trackParam.cmpFlag = (*varPtr > commandArg2) ;
            break;

        case MmlCommand::MML_CMP_LE:
            trackParam.cmpFlag = (*varPtr <= commandArg2) ;
            break;

        case MmlCommand::MML_CMP_LT:
            trackParam.cmpFlag = (*varPtr < commandArg2) ;
            break;

        case MmlCommand::MML_CMP_NE:
            trackParam.cmpFlag = (*varPtr != commandArg2) ;
            break;

        case MmlCommand::MML_USERPROC:
            //player->CallSequenceUserprocCallback(
            //    static_cast<u16>(commandArg1), // procId
            //    track
            //);
            break;

        // Modulation 2 {{
        case MmlCommand::MML_MOD_2_DEPTH:
            trackParam.lfoParam[1].depth = static_cast<u8>(commandArg1) / 128.0f;
            break;

        case MmlCommand::MML_MOD_2_SPEED:
            trackParam.lfoParam[1].speed = static_cast<u8>(commandArg1) * MOD_SPEED_BASE;
            break;

        case MmlCommand::MML_MOD_2_PERIOD:
            {
                s16 arg = static_cast<s16>(commandArg1);
                if (arg == 0)
                {
                    trackParam.lfoParam[1].speed = 0.0f;
                }
                else
                {
                    trackParam.lfoParam[1].speed = 100.f / arg;
                }
            }
            break;

        case MmlCommand::MML_MOD_2_PHASE:
            trackParam.lfoParam[1].phase = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MOD_2_TYPE:
            trackParam.lfoTarget[1] = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MOD_2_RANGE:
            trackParam.lfoParam[1].range = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MOD_2_DELAY:
            trackParam.lfoParam[1].delay = static_cast<u32>(commandArg1 * 5); // in 5ms units
            break;

        case MmlCommand::MML_MOD_2_CURVE:
            trackParam.lfoParam[1].curve = static_cast<u8>(commandArg1);
            break;
        // }}
        // Modulation 3 {{
        case MmlCommand::MML_MOD_3_DEPTH:
            trackParam.lfoParam[2].depth = static_cast<u8>(commandArg1) / 128.0f;
            break;

        case MmlCommand::MML_MOD_3_SPEED:
            trackParam.lfoParam[2].speed = static_cast<u8>(commandArg1) * MOD_SPEED_BASE;
            break;

        case MmlCommand::MML_MOD_3_PERIOD:
            {
                s16 arg = static_cast<s16>(commandArg1);
                if (arg == 0)
                {
                    trackParam.lfoParam[2].speed = 0.0f;
                }
                else
                {
                    trackParam.lfoParam[2].speed = 100.f / arg;
                }
            }
            break;

        case MmlCommand::MML_MOD_3_PHASE:
            trackParam.lfoParam[2].phase = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MOD_3_TYPE:
            trackParam.lfoTarget[2] = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MOD_3_RANGE:
            trackParam.lfoParam[2].range = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MOD_3_DELAY:
            trackParam.lfoParam[2].delay = static_cast<u32>(commandArg1 * 5); // in 5ms units
            break;

        case MmlCommand::MML_MOD_3_CURVE:
            trackParam.lfoParam[2].curve = static_cast<u8>(commandArg1);
            break;
        // }}
        // Modulation 4 {{
        case MmlCommand::MML_MOD_4_DEPTH:
            trackParam.lfoParam[3].depth = static_cast<u8>(commandArg1) / 128.0f;
            break;

        case MmlCommand::MML_MOD_4_SPEED:
            trackParam.lfoParam[3].speed = static_cast<u8>(commandArg1) * MOD_SPEED_BASE;
            break;

        case MmlCommand::MML_MOD_4_PERIOD:
            {
                s16 arg = static_cast<s16>(commandArg1);
                if (arg == 0)
                {
                    trackParam.lfoParam[3].speed = 0.0f;
                }
                else
                {
                    trackParam.lfoParam[3].speed = 100.f / arg;
                }
            }
            break;

        case MmlCommand::MML_MOD_4_PHASE:
            trackParam.lfoParam[3].phase = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MOD_4_TYPE:
            trackParam.lfoTarget[3] = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MOD_4_RANGE:
            trackParam.lfoParam[3].range = static_cast<u8>(commandArg1);
            break;

        case MmlCommand::MML_MOD_4_DELAY:
            trackParam.lfoParam[3].delay = static_cast<u32>(commandArg1 * 5); // in 5ms units
            break;

        case MmlCommand::MML_MOD_4_CURVE:
            trackParam.lfoParam[3].curve = static_cast<u8>(commandArg1);
            break;
        // }}

        } // switch (cmdex)
    }
}

void MmlParser::noteOnCommandProc(SequenceTrack* track, s32 key, s32 velocity, s32 length, bool tieFlag) const
{
    track->noteOn(key, velocity, length, tieFlag);
}

bool sSwapLittleSeqArgs = true; //? For future BCSAR support

u16 MmlParser::Read16(const u8** ptr)
{
    if (sFileEndian == sead::Endian::eLittle && sSwapLittleSeqArgs)
    {
        u16 ret = ReadByte(ptr);
        ret |= (ReadByte(ptr) << 8);
        return ret;
    }
    else
    {
        u16 ret = ReadByte(ptr);
        ret <<= 8;
        ret |= ReadByte(ptr);
        return ret;
    }
}

u32 MmlParser::Read24(const u8** ptr)
{
    if (sFileEndian == sead::Endian::eLittle && sSwapLittleSeqArgs)
    {
        u32 ret = ReadByte(ptr);
        ret |= (ReadByte(ptr) << 8);
        ret |= (ReadByte(ptr) << 16);
        return ret;
    }
    else
    {
        u32 ret = ReadByte(ptr);
        ret <<= 8;
        ret |= ReadByte(ptr);
        ret <<= 8;
        ret |= ReadByte(ptr);
        return ret;
    }
}

s32 MmlParser::ReadVar(const u8** ptr)
{
    s32 ret = 0;
    u8 b;
    int i;

    for(i = 0 ; ; ++i) {
        SEAD_ASSERT(i < 4);
        b = ReadByte(ptr);
        ret <<= 7;
        ret |= b & 0x7f;
        if (! (b & 0x80)) break;
    }

    return ret;
}

s32 MmlParser::ReadArg(const u8** ptr, SequenceSoundPlayer* player, SequenceTrack* track, SeqArgType argType)
{
    s32 var = 0;

    switch (argType) {

    case SEQ_ARG_U8:
        var = ReadByte(ptr);
        break;

    case SEQ_ARG_S16:
        var = Read16(ptr);
        break;

    case SEQ_ARG_VMIDI:
        var = ReadVar(ptr);
        break;

    case SEQ_ARG_VARIABLE: {
        u8 varNo = ReadByte(ptr);
        const volatile s16* varPtr = GetVariablePtr(player, track, varNo);
        if (varPtr != NULL) {
            var = *varPtr;
        }
        break;
    }

    case SEQ_ARG_RANDOM: {
        s32 rand;
        s16 min;
        s16 max;

        min = static_cast<s16>(Read16(ptr));
        max = static_cast<s16>(Read16(ptr));

        rand = sead::GlobalRandom::instance()->getU32(0xFFFF);  /* 0x0000 - 0xffff */
        rand *= (max - min) + 1;
        rand >>= 16;
        rand += min;
        var = rand;
        break;
    }

    default:
        break;
    }

    return var;
}

volatile s16* MmlParser::GetVariablePtr(SequenceSoundPlayer* player, SequenceTrack* track, int varNo)
{
    SEAD_ASSERT(0 <= varNo &&
                varNo < SequenceSoundPlayer::cPlayerVariableNum + SequenceSoundPlayer::cGlobalVariableNum + SequenceTrack::cTrackVariableNum
   );

    if (varNo < SequenceSoundPlayer::cPlayerVariableNum
               + SequenceSoundPlayer::cGlobalVariableNum)
    {
        return player->getVariablePtr(varNo);
    }
    else if (varNo < SequenceSoundPlayer::cPlayerVariableNum
                    + SequenceSoundPlayer::cGlobalVariableNum
                    + SequenceTrack::cTrackVariableNum)
    {
        return track->getVariablePtr(varNo - SequenceSoundPlayer::cPlayerVariableNum - SequenceSoundPlayer::cGlobalVariableNum);
    }
    else
    {
        return NULL;
    }
}

u32 MmlParser::ParseAllocTrack( const void* baseAddress, u32 seqOffset, u32* allocTrack )
{
    SEAD_ASSERT( baseAddress );
    SEAD_ASSERT( allocTrack );

    const u8* ptr = static_cast<const u8*>( sead::PtrUtil::addOffset( baseAddress, seqOffset ) );
    if ( *ptr != MmlCommand::MML_ALLOC_TRACK )
    {
        *allocTrack = (1 << 0);
        return seqOffset;
    }
    else
    {
        ++ptr;
        u32 tracks = *ptr;
        tracks <<= 8;
        ++ptr;
        tracks |= *ptr;
        tracks |= (1 << 0);
        *allocTrack = tracks;
        return seqOffset + 3;
    }
}
