#pragma once

#include "Adpcm.h"

#include <basis/seadAssert.h>
#include <basis/seadTypes.h>
#include <prim/seadEndian.h>

namespace snd {

typedef void (*SoundFrameUserCallback)(void* arg);

enum class OutputMode
{
    Mono,
    Stereo,
    Num
};

enum class OutputLineIdx
{
    Main,
    ReservedMax,
    User0 = 16,
    Max = 32
};

enum class OutputLine
{
    Main = 1 << (u32)OutputLineIdx::Main,
    ReservedMax = 1 << (u32)OutputLineIdx::ReservedMax,
    User0 = 1 << (u32)OutputLineIdx::User0
};

enum class AuxBus
{
    A,
    B,
    C,
    Num
};

enum class SampleFormat
{
    PcmS8,
    PcmS16,
    DspAdpcm,
    PcmS32
};

enum class ChannelIdx
{
    Left,
    Right,
    Num
};

static const u32 cWaveChannelMax = 2;

enum class PanMode
{
    Dual,
    Balance,
    Invalid
};

enum class PanCurve
{
    Sqrt,
    Sqrt0Db,
    Sqrt0DbClamp,
    SinCos,
    SinCos0Db,
    SinCos0DbClamp,
    Linear,
    Linear0Db,
    Linear0DbClamp,
    Invalid
};

enum class WaveType
{
    NwWav,
    DspAdpcm,
    Invalid = -1
};

//! Need to swap endiandess when reading
struct DspAdpcmParam
{
    u16 coef[8][2];
    u16 predScale;
    u16 yn1;
    u16 yn2;
};

struct AdshrCurve
{
    AdshrCurve( u8 a = 0, u8 d = 0, u8 s = 0, u8 h = 0, u8 r = 0 )
        : attack(a)
        , decay(d)
        , sustain(s)
        , hold(h)
        , release(r)
    {
    }

    u8 attack;
    u8 decay;
    u8 sustain;
    u8 hold;
    u8 release;
};

enum class BiquadFilterType
{
    Inherit = -1,

    None = 0,
    Lpf = 1,
    Hpf = 2,
    Bpf512 = 3,
    Bpf1024 = 4,
    Bpf2048 = 5,

    UserMin = 64,

    User0 = UserMin,

    UserMax = User0,

    DataMin = None,

    Min = Inherit,

    Max = UserMax
};

enum class SrcType
{
    Type_None,
    Type_Linear,
    Type_4Tap
};

struct OutputMix
{
    f32 mainBus[(u32)ChannelIdx::Num];
    f32 auxBus[(u32)AuxBus::Num][(u32)ChannelIdx::Num];
};

struct WaveBuffer
{
    enum class Status
    {
        Free,
        Wait,
        Play,
        Done
    };

    WaveBuffer()
        : status(Status::Free)
    {
        this->initialize();
    }

    ~WaveBuffer()
    {
        SEAD_ASSERT(status == Status::Free || status == Status::Done);
    }

    void initialize()
    {
        this->bufferAddress = nullptr;
        this->sampleLength = 0;
        this->sampleOffset = 0;
        this->adpcmContext = nullptr;
        this->userParam = nullptr;
        this->loopFlag = false;
        this->status = Status::Free;
        this->next = nullptr;

        this->endian = sead::Endian::eBig;
    }

    const void* bufferAddress;
    u32 sampleLength;
    u32 sampleOffset;
    const AdpcmContext* adpcmContext;
    void* userParam;
    bool loopFlag;
    Status status;
    WaveBuffer* next;

    sead::Endian::Types endian; // Custom
};

struct BiquadFilterCoefficients
{
    s16 b0;
    s16 b1;
    s16 b2;
    s16 a1;
    s16 a2;
};

struct FinalMixData
{
    FinalMixData()
        : left(nullptr)
        , right(nullptr)
        , sampleCount(0)
        , channelCount(0)
    {
    }

    f32* left;
    f32* right;

    u16 sampleCount;
    u16 channelCount;
};

enum class MixMode
{
    Pan,
    MixParameter,
    Num
};

struct MixParameter
{
    union
    {
        struct
        {
            f32 fL;
            f32 fR;
        };

        f32 ch[(u32)ChannelIdx::Num];
    };

    MixParameter()
        : fL(1.0f)
        , fR(1.0f)
    {
    }

    MixParameter(f32 _fL, f32 _fR)
        : fL(_fL)
        , fR(_fR)
    {
    }
};

enum class UpdateType
{
    AudioFrame,
    GameFrame
};

namespace internal {

//! Need to swap endiandess when reading
struct DspAdpcmLoopParam
{
    u16 loopPredScale;
    u16 loopYn1;
    u16 loopYn2;
};

struct WaveInfo
{
    SampleFormat sampleFormat;
    bool loopFlag;
    s32 channelCount;
    s32 sampleRate;
    u32 loopStartFrame;
    u32 loopEndFrame;
    u32 originalLoopStartFrame;

    struct ChannelParam
    {
        const void* dataAddress;
        DspAdpcmParam adpcmParam;
        DspAdpcmLoopParam adpcmLoopParam;
    } channelParam[cWaveChannelMax];

    sead::Endian::Types endian = sead::Endian::eBig; // Custom
};

struct OutputParam
{
    f32 volume;
    u32 mixMode;
    MixParameter mixParameter[cWaveChannelMax];
    f32 pan;
    //f32 span;
    f32 mainSend;
    f32 fxSend[(u32)AuxBus::Num];

    void initialize()
    {
        volume = 1.0f;
        mixMode = 0;
        pan = 0.0f;
        //span = 0.0f;
        mainSend = 0.0f;

        for (u32 i = 0; i < (u32)AuxBus::Num; i++)
        {
            fxSend[i] = 0.0f;
        }
    };
};

enum class VoiceState
{
    Play,
    Stop,
    Pause
};

struct VoiceParam
{
    f32 volume;
    f32 pitch;
    OutputMix mix;

    bool monoFilterFlag;
    bool biquadFilterFlag;

    BiquadFilterCoefficients biquadFilterCoefficients;
    u16 monoFilterCutoff; // LPF cut-off frequency.
    u8 interpolationType; // Interpolation type.

    void initialize();
};

struct VoiceInfo
{
    VoiceState voiceState;
    WaveBuffer::Status waveBufferStatus;
    void* waveBufferTag;
    u32 playPosition;
    void* userId;
};

} // namespace snd::internal

} // namespace snd
