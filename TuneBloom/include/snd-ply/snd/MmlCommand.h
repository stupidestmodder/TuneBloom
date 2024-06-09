#pragma once

class MmlCommand
{
public:

    enum Mml
    {
        // Variable length parameter commands.
        MML_WAIT            = 0x80,
        MML_PRG             = 0x81,

        // Control commands.
        MML_OPEN_TRACK      = 0x88,
        MML_JUMP            = 0x89,
        MML_CALL            = 0x8a,

        // prefix commands
        MML_RANDOM          = 0xa0,
        MML_VARIABLE        = 0xa1,
        MML_IF              = 0xa2,
        MML_TIME            = 0xa3,
        MML_TIME_RANDOM     = 0xa4,
        MML_TIME_VARIABLE   = 0xa5,

        // u8 parameter commands.
        MML_TIMEBASE        = 0xb0,
        MML_ENV_HOLD        = 0xb1,
        MML_MONOPHONIC      = 0xb2,
        MML_VELOCITY_RANGE  = 0xb3,
        MML_BIQUAD_TYPE     = 0xb4,
        MML_BIQUAD_VALUE    = 0xb5,
        MML_BANK_SELECT     = 0xb6,
                                // Six b7-bc are open.
        MML_MOD_PHASE       = 0xbd,
        MML_MOD_CURVE       = 0xbe,
        MML_FRONT_BYPASS    = 0xbf,
        MML_PAN             = 0xc0,
        MML_VOLUME          = 0xc1,
        MML_MAIN_VOLUME     = 0xc2,
        MML_TRANSPOSE       = 0xc3,
        MML_PITCH_BEND      = 0xc4,
        MML_BEND_RANGE      = 0xc5,
        MML_PRIO            = 0xc6,
        MML_NOTE_WAIT       = 0xc7,
        MML_TIE             = 0xc8,
        MML_PORTA           = 0xc9,
        MML_MOD_DEPTH       = 0xca,
        MML_MOD_SPEED       = 0xcb,
        MML_MOD_TYPE        = 0xcc,
        MML_MOD_RANGE       = 0xcd,
        MML_PORTA_SW        = 0xce,
        MML_PORTA_TIME      = 0xcf,
        MML_ATTACK          = 0xd0,
        MML_DECAY           = 0xd1,
        MML_SUSTAIN         = 0xd2,
        MML_RELEASE         = 0xd3,
        MML_LOOP_START      = 0xd4,
        MML_VOLUME2         = 0xd5,
        MML_PRINTVAR        = 0xd6,
        MML_SURROUND_PAN    = 0xd7,
        MML_LPF_CUTOFF      = 0xd8,
        MML_FXSEND_A        = 0xd9,
        MML_FXSEND_B        = 0xda,
        MML_MAINSEND        = 0xdb,
        MML_INIT_PAN        = 0xdc,
        MML_MUTE            = 0xdd,
        MML_FXSEND_C        = 0xde,
        MML_DAMPER          = 0xdf,

        // s16 parameter commands.
        MML_MOD_DELAY       = 0xe0,
        MML_TEMPO           = 0xe1,
        MML_SWEEP_PITCH     = 0xe3,
        MML_MOD_PERIOD      = 0xe4,

        // Extended command
        MML_EX_COMMAND      = 0xf0,

        // Other.
        MML_ENV_RESET       = 0xfb,
        MML_LOOP_END        = 0xfc,
        MML_RET             = 0xfd,
        MML_ALLOC_TRACK     = 0xfe,
        MML_FIN             = 0xff
    };

    enum MmlEx
    {
        MML_SETVAR          = 0x80,
        MML_ADDVAR          = 0x81,
        MML_SUBVAR          = 0x82,
        MML_MULVAR          = 0x83,
        MML_DIVVAR          = 0x84,
        MML_SHIFTVAR        = 0x85,
        MML_RANDVAR         = 0x86,
        MML_ANDVAR          = 0x87,
        MML_ORVAR           = 0x88,
        MML_XORVAR          = 0x89,
        MML_NOTVAR          = 0x8a,
        MML_MODVAR          = 0x8b,

        MML_CMP_EQ          = 0x90,
        MML_CMP_GE          = 0x91,
        MML_CMP_GT          = 0x92,
        MML_CMP_LE          = 0x93,
        MML_CMP_LT          = 0x94,
        MML_CMP_NE          = 0x95,

        MML_MOD_2_CURVE     = 0xa0,
        MML_MOD_2_PHASE     = 0xa1,
        MML_MOD_2_DEPTH     = 0xa2,
        MML_MOD_2_SPEED     = 0xa3,
        MML_MOD_2_TYPE      = 0xa4,
        MML_MOD_2_RANGE     = 0xa5,

        MML_MOD_3_CURVE     = 0xa6,
        MML_MOD_3_PHASE     = 0xa7,
        MML_MOD_3_DEPTH     = 0xa8,
        MML_MOD_3_SPEED     = 0xa9,
        MML_MOD_3_TYPE      = 0xaa,
        MML_MOD_3_RANGE     = 0xab,

        MML_MOD_4_CURVE     = 0xac,
        MML_MOD_4_PHASE     = 0xad,
        MML_MOD_4_DEPTH     = 0xae,
        MML_MOD_4_SPEED     = 0xaf,
        MML_MOD_4_TYPE      = 0xb0,
        MML_MOD_4_RANGE     = 0xb1,

        MML_USERPROC        = 0xe0,
        MML_MOD_2_DELAY     = 0xe1,
        MML_MOD_2_PERIOD    = 0xe2,
        MML_MOD_3_DELAY     = 0xe3,
        MML_MOD_3_PERIOD    = 0xe4,
        MML_MOD_4_DELAY     = 0xe5,
        MML_MOD_4_PERIOD    = 0xe6
    };
};
