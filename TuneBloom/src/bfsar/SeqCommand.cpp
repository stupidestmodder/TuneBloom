#include <bfsar/SeqCommand.h>

#include <snd/snd_SequenceSoundFileReader.h>

SeqArgBase* nw__snd__internal__driver__MmlParser__ReadArg(const u8** ptr, MmlParser::SeqArgType argType, bool hasSign = false)
{
    switch (argType)
    {
        case MmlParser::SEQ_ARG_U8:
            return new SeqArg8(*ptr, hasSign);
        case MmlParser::SEQ_ARG_S16:
            return new SeqArg16(*ptr, hasSign);
        case MmlParser::SEQ_ARG_VMIDI:
            return new SeqArgVMIDI(*ptr);
        case MmlParser::SEQ_ARG_VARIABLE:
            return new SeqArgVariable(*ptr);
        case MmlParser::SEQ_ARG_RANDOM:
            return new SeqArgRandom(*ptr);
    }

    return nullptr;
}

const char* const MmlCommandNote::sKeys[MmlCommandNote::sKeysNum] = {
    "cnm1", "csm1", "dnm1", "dsm1", "enm1", "fnm1", "fsm1", "gnm1",
    "gsm1", "anm1", "asm1", "bnm1", "cn0",  "cs0",  "dn0",  "ds0",
    "en0",  "fn0",  "fs0",  "gn0",  "gs0",  "an0",  "as0",  "bn0",
    "cn1",  "cs1",  "dn1",  "ds1",  "en1",  "fn1",  "fs1",  "gn1",
    "gs1",  "an1",  "as1",  "bn1",  "cn2",  "cs2",  "dn2",  "ds2",
    "en2",  "fn2",  "fs2",  "gn2",  "gs2",  "an2",  "as2",  "bn2",
    "cn3",  "cs3",  "dn3",  "ds3",  "en3",  "fn3",  "fs3",  "gn3",
    "gs3",  "an3",  "as3",  "bn3",  "cn4",  "cs4",  "dn4",  "ds4",
    "en4",  "fn4",  "fs4",  "gn4",  "gs4",  "an4",  "as4",  "bn4",
    "cn5",  "cs5",  "dn5",  "ds5",  "en5",  "fn5",  "fs5",  "gn5",
    "gs5",  "an5",  "as5",  "bn5",  "cn6",  "cs6",  "dn6",  "ds6",
    "en6",  "fn6",  "fs6",  "gn6",  "gs6",  "an6",  "as6",  "bn6",
    "cn7",  "cs7",  "dn7",  "ds7",  "en7",  "fn7",  "fs7",  "gn7",
    "gs7",  "an7",  "as7",  "bn7",  "cn8",  "cs8",  "dn8",  "ds8",
    "en8",  "fn8",  "fs8",  "gn8",  "gs8",  "an8",  "as8",  "bn8",
    "cn9",  "cs9",  "dn9",  "ds9",  "en9",  "fn9",  "fs9",  "gn9"
};

const char* const MmlCommandModType::sTypes[] = {
    "MOD_TYPE_PITCH",
    "MOD_TYPE_VOLUME",
    "MOD_TYPE_PAN"
};

MmlCommandBase* nw__snd__internal__driver__MmlParser__Parse(const u8*& trackData, nw::snd::internal::SequenceSoundFileReader& reader)
{
    MmlParser::SeqArgType argType = MmlParser::SEQ_ARG_NONE;
    MmlParser::SeqArgType argType2 = MmlParser::SEQ_ARG_NONE;
    bool useArgType = false;
    bool conditional = false;

    MmlCommandBase* cmdInst = nullptr;

    const u8* start = trackData;

    u32 cmd = MmlParser::MmlParser::ReadByte(&trackData);

    // processing for the prefix binary
    if (cmd == MmlCommand::MML_IF)
    {
        cmd = MmlParser::ReadByte(&trackData);
        conditional = true;
    }

    // second parameter
    if (cmd == MmlCommand::MML_TIME)
    {
        cmd = MmlParser::ReadByte(&trackData);
        argType2 = MmlParser::SEQ_ARG_S16;
    }
    else if (cmd == MmlCommand::MML_TIME_RANDOM)
    {
        cmd = MmlParser::ReadByte(&trackData);
        argType2 = MmlParser::SEQ_ARG_RANDOM;
    }
    else if (cmd == MmlCommand::MML_TIME_VARIABLE)
    {
        cmd = MmlParser::ReadByte(&trackData);
        argType2 = MmlParser::SEQ_ARG_VARIABLE;
    }

    // Parameters.
    if (cmd == MmlCommand::MML_RANDOM)
    {
        cmd = MmlParser::ReadByte(&trackData);
        argType = MmlParser::SEQ_ARG_RANDOM;
        useArgType = true;
    }
    else if (cmd == MmlCommand::MML_VARIABLE)
    {
        cmd = MmlParser::ReadByte(&trackData);
        argType = MmlParser::SEQ_ARG_VARIABLE;
        useArgType = true;
    }

    if ((cmd & 0x80) == 0)
    {
        // 'Note' command
        const u8 velocity = MmlParser::ReadByte(&trackData);
        SeqArgBase* length = nw__snd__internal__driver__MmlParser__ReadArg(
            &trackData,
            useArgType ? argType : MmlParser::SEQ_ARG_VMIDI
        );

        cmdInst = new MmlCommandNote(cmd, velocity, length, conditional);
    }
    else
    {
        SeqArgBase* arg;
        SeqArgBase* arg1;
        SeqArgBase* arg2;

        switch (cmd & 0xf0)
        {
        case 0x80:
        {
            switch (cmd)
            {
            case MmlCommand::MML_WAIT:
            {
                // Process here.
                arg = nw__snd__internal__driver__MmlParser__ReadArg(
                    &trackData,
                    useArgType ? argType : MmlParser::SEQ_ARG_VMIDI
                );
                cmdInst = new MmlCommandWait(arg, conditional);
                break;
            }
            case MmlCommand::MML_PRG:
            {
                arg = nw__snd__internal__driver__MmlParser__ReadArg(
                    &trackData,
                    useArgType ? argType : MmlParser::SEQ_ARG_VMIDI
                );
                cmdInst = new MmlCommandPrg(arg, conditional);
                break;
            }
            case MmlCommand::MML_OPEN_TRACK:
            {
                u8 trackNo = MmlParser::ReadByte(&trackData);
                u32 offset = MmlParser::Read24(&trackData);
                const char* label = reader.getLabelByOffsetFromCache(offset);
                cmdInst = new MmlCommandOpenTrack(trackNo, LabelObj(offset, label), conditional);
                break;
            }
            case MmlCommand::MML_JUMP:
            {
                u32 offset = MmlParser::Read24(&trackData);
                const char* label = reader.getLabelByOffsetFromCache(offset);
                cmdInst = new MmlCommandJump(LabelObj(offset, label), conditional);
                break;
            }
            case MmlCommand::MML_CALL:
            {
                u32 offset = MmlParser::Read24(&trackData);
                const char* label = reader.getLabelByOffsetFromCache(offset);
                cmdInst = new MmlCommandCall(LabelObj(offset, label), conditional);
                break;
            }
            }
            break; // case 0x88 - 0x8f
        }

        case 0xb0: case 0xc0: case 0xd0: // u8 parameters (mixed in places with s8 parameters)
        {
            arg1 = nw__snd__internal__driver__MmlParser__ReadArg(
                &trackData,
                useArgType ? argType : MmlParser::SEQ_ARG_U8,
                cmd == MmlCommand::MML_TRANSPOSE || cmd == MmlCommand::MML_PITCH_BEND
            );

            arg2 = argType2 != MmlParser::SEQ_ARG_NONE ? nw__snd__internal__driver__MmlParser__ReadArg(&trackData, argType2, true) : nullptr;

            switch (cmd)
            {
                case MmlCommand::MML_TIMEBASE:
                    cmdInst = new MmlCommandTimebase(arg1, conditional);
                    break;
                case MmlCommand::MML_ENV_HOLD:
                    cmdInst = new MmlCommandEnvHold(arg1, conditional);
                    break;
                case MmlCommand::MML_MONOPHONIC:
                    cmdInst = new MmlCommandMonophonic(arg1, conditional);
                    break;
                case MmlCommand::MML_VELOCITY_RANGE:
                    cmdInst = new MmlCommandVelocityRange(arg1, conditional);
                    break;
                case MmlCommand::MML_BIQUAD_TYPE:
                    cmdInst = new MmlCommandBiquadType(arg1, conditional);
                    break;
                case MmlCommand::MML_BIQUAD_VALUE:
                    cmdInst = new MmlCommandBiquadValue(arg1, conditional);
                    break;
                case MmlCommand::MML_BANK_SELECT:
                    cmdInst = new MmlCommandBankSelect(arg1, conditional);
                    break;
                //case MmlCommand::MML_MOD_PHASE:
                //case MmlCommand::MML_MOD_CURVE:
                case MmlCommand::MML_FRONT_BYPASS:
                    cmdInst = new MmlCommandFrontBypass(arg1, conditional);
                    break;
                case MmlCommand::MML_PAN:
                    cmdInst = new MmlCommandPan(arg1, arg2, conditional);
                    break;
                case MmlCommand::MML_VOLUME:
                    cmdInst = new MmlCommandVolume(arg1, arg2, conditional);
                    break;
                case MmlCommand::MML_MAIN_VOLUME:
                    cmdInst = new MmlCommandMainVolume(arg1, arg2, conditional);
                    break;
                case MmlCommand::MML_TRANSPOSE:
                    cmdInst = new MmlCommandTranspose(arg1, conditional);
                    break;
                case MmlCommand::MML_PITCH_BEND:
                    cmdInst = new MmlCommandPitchBend(arg1, arg2, conditional);
                    break;
                case MmlCommand::MML_BEND_RANGE:
                    cmdInst = new MmlCommandBendRange(arg1, conditional);
                    break;
                case MmlCommand::MML_PRIO:
                    cmdInst = new MmlCommandPrio(arg1, conditional);
                    break;
                case MmlCommand::MML_NOTE_WAIT:
                    cmdInst = new MmlCommandNoteWait(arg1, conditional);
                    break;
                case MmlCommand::MML_TIE:
                    cmdInst = new MmlCommandTie(arg1, conditional);
                    break;
                case MmlCommand::MML_PORTA:
                    cmdInst = new MmlCommandPorta(arg1, conditional);
                    break;
                case MmlCommand::MML_MOD_DEPTH:
                    cmdInst = new MmlCommandModDepth(arg1, conditional);
                    break;
                case MmlCommand::MML_MOD_SPEED:
                    cmdInst = new MmlCommandModSpeed(arg1, conditional);
                    break;
                case MmlCommand::MML_MOD_TYPE:
                    cmdInst = new MmlCommandModType(arg1, conditional);
                    break;
                case MmlCommand::MML_MOD_RANGE:
                    cmdInst = new MmlCommandModRange(arg1, conditional);
                    break;
                case MmlCommand::MML_PORTA_SW:
                    cmdInst = new MmlCommandPortaSw(arg1, conditional);
                    break;
                case MmlCommand::MML_PORTA_TIME:
                    cmdInst = new MmlCommandPortaTime(arg1, conditional);
                    break;
                case MmlCommand::MML_ATTACK:
                    cmdInst = new MmlCommandAttack(arg1, conditional);
                    break;
                case MmlCommand::MML_DECAY:
                    cmdInst = new MmlCommandDecay(arg1, conditional);
                    break;
                case MmlCommand::MML_SUSTAIN:
                    cmdInst = new MmlCommandSustain(arg1, conditional);
                    break;
                case MmlCommand::MML_RELEASE:
                    cmdInst = new MmlCommandRelease(arg1, conditional);
                    break;
                case MmlCommand::MML_LOOP_START:
                    cmdInst = new MmlCommandLoopStart(arg1, conditional);
                    break;
                case MmlCommand::MML_VOLUME2:
                    cmdInst = new MmlCommandVolume2(arg1, arg2, conditional);
                    break;
                case MmlCommand::MML_PRINTVAR:
                    cmdInst = new MmlCommandPrintVar(arg1, conditional);
                    break;
                case MmlCommand::MML_SURROUND_PAN:
                    cmdInst = new MmlCommandSurroundPan(arg1, arg2, conditional);
                    break;
                case MmlCommand::MML_LPF_CUTOFF:
                    cmdInst = new MmlCommandLPFCutoff(arg1, conditional);
                    break;
                case MmlCommand::MML_FXSEND_A:
                    cmdInst = new MmlCommandFxSendA(arg1, conditional);
                    break;
                case MmlCommand::MML_FXSEND_B:
                    cmdInst = new MmlCommandFxSendB(arg1, conditional);
                    break;
                case MmlCommand::MML_MAINSEND:
                    cmdInst = new MmlCommandMainSend(arg1, conditional);
                    break;
                case MmlCommand::MML_INIT_PAN:
                    cmdInst = new MmlCommandInitPan(arg1, conditional);
                    break;
                case MmlCommand::MML_MUTE:
                    cmdInst = new MmlCommandMute(arg1, conditional);
                    break;
                case MmlCommand::MML_FXSEND_C:
                    cmdInst = new MmlCommandFxSendC(arg1, conditional);
                    break;
                case MmlCommand::MML_DAMPER:
                    cmdInst = new MmlCommandDamper(arg1, conditional);
                    break;
            }

            break;
        }

        case 0x90: // Extra
        {
            SEAD_ASSERT_MSG(false, "Command type does not exist in current version: 0x%02X", cmd);
            break;
        }

        case 0xe0: // s16 parameters
        {
            arg = nw__snd__internal__driver__MmlParser__ReadArg(
                &trackData,
                useArgType ? argType : MmlParser::SEQ_ARG_S16,
                true
            );

            switch (cmd)
            {
                case MmlCommand::MML_MOD_DELAY:
                    cmdInst = new MmlCommandModDelay(arg, conditional);
                    break;
                case MmlCommand::MML_TEMPO:
                    cmdInst = new MmlCommandTempo(arg, conditional);
                    break;
                case MmlCommand::MML_SWEEP_PITCH:
                    cmdInst = new MmlCommandSweepPitch(arg, conditional);
                    break;
                case MmlCommand::MML_MOD_PERIOD:
                    cmdInst = new MmlCommandModPeriod(arg, conditional);
                    break;
            }

            break;
        }

        case 0xf0: // either no parameters, or extension command
        {
            switch (cmd)
            {
            case MmlCommand::MML_ALLOC_TRACK:
                arg = nw__snd__internal__driver__MmlParser__ReadArg(&trackData, MmlParser::SEQ_ARG_S16);
                cmdInst = new MmlCommandAllocTrack(arg);
                break;

            case MmlCommand::MML_FIN:
                cmdInst = new MmlCommandFin(conditional);
                break;

            case MmlCommand::MML_EX_COMMAND:
            {
                u32 cmdex = MmlParser::ReadByte(&trackData);

                switch (cmdex & 0xf0)
                {
                case 0xa0: case 0xb0: // u8 parameters.
                {
                    //SEAD_ASSERT_MSG(false, "Command type does not exist in current version: 0x%02X", cmd);
                    break;
                }

                case 0xe0: // u16 parameter
                {
                    arg = nw__snd__internal__driver__MmlParser__ReadArg(
                        &trackData,
                        useArgType ? argType : MmlParser::SEQ_ARG_S16
                    );
                    if (cmdex == MmlCommand::MML_USERPROC)
                        cmdInst = new MmlCommandUserproc(arg, conditional);
                    break;
                }

                case 0x80: case 0x90: // 2 parameters
                {
                    arg1 = nw__snd__internal__driver__MmlParser__ReadArg(&trackData, MmlParser::SEQ_ARG_VARIABLE);
                    arg2 = nw__snd__internal__driver__MmlParser__ReadArg(
                        &trackData,
                        useArgType ? argType : MmlParser::SEQ_ARG_S16,
                        true
                    );

                    switch (cmdex)
                    {
                        case MmlCommand::MML_SETVAR:
                            cmdInst = new MmlCommandSetVar(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_ADDVAR:
                            cmdInst = new MmlCommandAddVar(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_SUBVAR:
                            cmdInst = new MmlCommandSubVar(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_MULVAR:
                            cmdInst = new MmlCommandMulVar(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_DIVVAR:
                            cmdInst = new MmlCommandDivVar(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_SHIFTVAR:
                            cmdInst = new MmlCommandShiftVar(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_RANDVAR:
                            cmdInst = new MmlCommandRandVar(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_ANDVAR:
                            cmdInst = new MmlCommandAndVar(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_ORVAR:
                            cmdInst = new MmlCommandOrVar(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_XORVAR:
                            cmdInst = new MmlCommandXorVar(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_NOTVAR:
                            cmdInst = new MmlCommandNotVar(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_MODVAR:
                            cmdInst = new MmlCommandModVar(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_CMP_EQ:
                            cmdInst = new MmlCommandCmpEq(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_CMP_GE:
                            cmdInst = new MmlCommandCmpGe(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_CMP_GT:
                            cmdInst = new MmlCommandCmpGt(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_CMP_LE:
                            cmdInst = new MmlCommandCmpLe(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_CMP_LT:
                            cmdInst = new MmlCommandCmpLt(arg1, arg2, conditional);
                            break;
                        case MmlCommand::MML_CMP_NE:
                            cmdInst = new MmlCommandCmpNe(arg1, arg2, conditional);
                            break;
                    }

                    break;
                }
                }
                break;
            } // case MmlCommand::MML_EX_COMMAND

            case MmlCommand::MML_ENV_RESET:
                cmdInst = new MmlCommandEnvReset(conditional);
                break;
            case MmlCommand::MML_LOOP_END:
                cmdInst = new MmlCommandLoopEnd(conditional);
                break;
            case MmlCommand::MML_RET:
                cmdInst = new MmlCommandRet(conditional);
                break;
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

    //SEAD_ASSERT_MSG(cmdInst, "Unhandled command type: 0x%02X", cmd);

    if (cmdInst)
    {
        cmdInst->init();

        if (false)
        {
            const auto& bytes = cmdInst->encode();

            s32 ret = sead::MemUtil::compare(start, bytes.data(), bytes.size());
            SEAD_ASSERT(ret == 0);
        }
    }

    return cmdInst;
}
