#pragma 

#include <bfsar/SeqArg.h>

#include <snd/MmlCommand.h>

class MmlCommandBase
{
public:
    MmlCommandBase(bool conditional = false)
        : mConditional(conditional)
    {
    }

    virtual void init()
    {
    }

    virtual std::vector<u8> encode() = 0
    {
        if (mConditional)
            return { MmlCommand::MML_IF };

        return { };
    }

    virtual std::vector<std::string> getArgs_() const = 0;
    virtual std::string toString() const = 0;

    std::string toString_(const std::string& cmd, bool checkConditional = true) const
    {
        std::string ret = cmd;

        if (checkConditional && mConditional)
            ret += "_if";

        const auto& args = getArgs_();

        if (args.size() > 0)
            ret += " ";

        for (u32 i = 0; i < args.size(); i++)
        {
            ret += args[i];

            if (i < args.size() - 1)
                ret += ", ";
        }

        return ret;
    }

    bool mConditional;
};

class MmlCommandArgs0 : public MmlCommandBase
{
public:
    MmlCommandArgs0(bool conditional = false)
        : MmlCommandBase(conditional)
    {
    }

    virtual MmlCommand::Mml getCommand_() const = 0;
    virtual const char* getCommandString_() const = 0;

    virtual std::vector<u8> encode()
    {
        std::vector<u8> ret = MmlCommandBase::encode();
        ret.push_back(getCommand_());

        return ret;
    }

    std::vector<std::string> getArgs_() const override
    {
        return { };
    }

    std::string toString() const override
    {
        return toString_(getCommandString_());
    }
};

template <typename ArgDefaultType>
class MmlCommandArgs1 : public MmlCommandBase
{
public:
    MmlCommandArgs1(SeqArgBase* arg, bool conditional = false)
        : MmlCommandBase(conditional)
    {
        const ArgDefaultType* argDefaultType = sead::DynamicCast<ArgDefaultType>(arg);
        if (argDefaultType)
        {
            //SEAD_ASSERT(validateArgValueDefaultType_(argDefaultType));
        }
        else
        {
            SEAD_ASSERT(sead::IsDerivedTypes<SeqArgRandom>(arg) || sead::IsDerivedTypes<SeqArgVariable>(arg));
        }

        mArg = arg;
    }

    virtual bool validateArgValueDefaultType_(const ArgDefaultType* arg) const = 0;

    virtual MmlCommand::Mml getCommand_() const = 0;
    virtual const char* getCommandString_() const = 0;

    virtual std::vector<u8> encode()
    {
        std::vector<u8> ret = MmlCommandBase::encode();

        if (sead::IsDerivedTypes<SeqArgRandom>(mArg))
        {
            ret.push_back(MmlCommand::MML_RANDOM);
        }
        else if (sead::IsDerivedTypes<SeqArgVariable>(mArg))
        {
            ret.push_back(MmlCommand::MML_VARIABLE);
        }
        else
        {
            const ArgDefaultType* argDefaultType = sead::DynamicCast<ArgDefaultType>(mArg);
            SEAD_ASSERT(argDefaultType);
            SEAD_ASSERT(validateArgValueDefaultType_(argDefaultType));
        }

        ret.push_back(getCommand_());
        const auto& argBytes = mArg->encode();
        ret.insert(ret.end(), argBytes.begin(), argBytes.end());

        return ret;
    }

    std::vector<std::string> getArgs_() const override
    {
        return { mArg->toString() };
    }

    std::string toString() const override
    {
        std::string cmd = getCommandString_();

        if (sead::IsDerivedTypes<SeqArgRandom>(mArg))
        {
            cmd += "_r";
        }
        else if (sead::IsDerivedTypes<SeqArgVariable>(mArg))
        {
            cmd += "_v";
        }
        else
        {
            const ArgDefaultType* argDefaultType = sead::DynamicCast<ArgDefaultType>(mArg);
            SEAD_ASSERT(argDefaultType);
            SEAD_ASSERT(validateArgValueDefaultType_(argDefaultType));
        }

        return toString_(cmd);
    }

    SeqArgBase* mArg;
};

class MmlCommandBoolArg : public MmlCommandBase
{
public:
    MmlCommandBoolArg(SeqArgBase* arg, bool conditional = false)
        : MmlCommandBase(conditional)
    {
        SeqArg8* argDefaultType = sead::DynamicCast<SeqArg8>(arg);
        SEAD_ASSERT(argDefaultType);

        SEAD_ASSERT(validateArgValue_(argDefaultType));

        mArg = argDefaultType;
    }

    virtual bool validateArgValue_(const SeqArg8* arg) const
    {
        return arg->mValue == 0 || arg->mValue == 1;
    }

    virtual bool isTrue_(const SeqArg8* arg) const
    {
        return arg->mValue;
    }

    virtual MmlCommand::Mml getCommand_() const = 0;
    virtual const char* getCommandString_() const = 0;

    virtual std::vector<u8> encode()
    {
        SEAD_ASSERT(mArg);
        SEAD_ASSERT(validateArgValue_(mArg));

        std::vector<u8> ret = MmlCommandBase::encode();

        ret.push_back(getCommand_());
        const auto& argBytes = mArg->encode();
        ret.insert(ret.end(), argBytes.begin(), argBytes.end());

        return ret;
    }

    std::vector<std::string> getArgs_() const override
    {
        return { };
    }

    std::string toString() const override
    {
        SEAD_ASSERT(mArg);
        SEAD_ASSERT(validateArgValue_(mArg));

        std::string cmd = getCommandString_();
        cmd += isTrue_(mArg) ? "on" : "off";

        return toString_(cmd);
    }

    SeqArg8* mArg;
};

template <typename Arg1DefaultType>
class MmlCommandArgs2 : public MmlCommandBase
{
public:
    MmlCommandArgs2(SeqArgBase* arg1, SeqArgBase* arg2 = nullptr, bool conditional = false)
        : MmlCommandBase(conditional)
    {
        const Arg1DefaultType* arg1DefaultType = sead::DynamicCast<Arg1DefaultType>(arg1);
        if (arg1DefaultType)
        {
            //SEAD_ASSERT(validateArg1ValueDefaultType_(arg1DefaultType));
        }
        else
        {
            SEAD_ASSERT(sead::IsDerivedTypes<SeqArgRandom>(arg1) || sead::IsDerivedTypes<SeqArgVariable>(arg1));
        }

        mArg1 = arg1;

        if (!arg2)
        {
            mArg2 = nullptr;
            return;
        }

        const SeqArg16* arg2DefaultType = sead::DynamicCast<SeqArg16>(arg2);
        if (arg2DefaultType)
        {
            SEAD_ASSERT(arg2DefaultType->mHasSign);
        }
        else
        {
            SEAD_ASSERT(sead::IsDerivedTypes<SeqArgRandom>(arg2) || sead::IsDerivedTypes<SeqArgVariable>(arg2));
        }

        mArg2 = arg2;
    }

    virtual bool validateArg1ValueDefaultType_(const Arg1DefaultType* arg1) const = 0;

    virtual MmlCommand::Mml getCommand_() const = 0;
    virtual const char* getCommandString_() const = 0;

    virtual std::vector<u8> encode()
    {
        std::vector<u8> ret = MmlCommandBase::encode();

        if (mArg2)
        {
            if (sead::IsDerivedTypes<SeqArgRandom>(mArg2))
            {
                ret.push_back(MmlCommand::MML_TIME_RANDOM);
            }
            else if (sead::IsDerivedTypes<SeqArgVariable>(mArg2))
            {
                ret.push_back(MmlCommand::MML_TIME_VARIABLE);
            }
            else
            {
                const SeqArg16* arg2DefaultType = sead::DynamicCast<SeqArg16>(mArg2);
                SEAD_ASSERT(arg2DefaultType);
                SEAD_ASSERT(arg2DefaultType->mHasSign);
                ret.push_back(MmlCommand::MML_TIME);
            }
        }

        if (sead::IsDerivedTypes<SeqArgRandom>(mArg1))
        {
            ret.push_back(MmlCommand::MML_RANDOM);
        }
        else if (sead::IsDerivedTypes<SeqArgVariable>(mArg1))
        {
            ret.push_back(MmlCommand::MML_VARIABLE);
        }
        else
        {
            const Arg1DefaultType* arg1DefaultType = sead::DynamicCast<Arg1DefaultType>(mArg1);
            SEAD_ASSERT(arg1DefaultType);
            SEAD_ASSERT(validateArg1ValueDefaultType_(arg1DefaultType));
        }

        ret.push_back(getCommand_());
        const auto& arg1Bytes = mArg1->encode();
        ret.insert(ret.end(), arg1Bytes.begin(), arg1Bytes.end());

        if (mArg2)
        {
            const auto& arg2Bytes = mArg2->encode();
            ret.insert(ret.end(), arg2Bytes.begin(), arg2Bytes.end());
        }

        return ret;
    }

    std::vector<std::string> getArgs_() const override
    {
        if (mArg2)
            return { mArg1->toString(), mArg2->toString() };

        return { mArg1->toString() };
    }

    std::string toString() const override
    {
        std::string cmd = getCommandString_();

        if (sead::IsDerivedTypes<SeqArgRandom>(mArg1))
        {
            cmd += "_r";
        }
        else if (sead::IsDerivedTypes<SeqArgVariable>(mArg1))
        {
            cmd += "_v";
        }
        else
        {
            const Arg1DefaultType* arg1DefaultType = sead::DynamicCast<Arg1DefaultType>(mArg1);
            SEAD_ASSERT(arg1DefaultType);
            SEAD_ASSERT(validateArg1ValueDefaultType_(arg1DefaultType));
        }

        if (mArg2)
        {
            if (sead::IsDerivedTypes<SeqArgRandom>(mArg2))
            {
                cmd += "_tr";
            }
            else if (sead::IsDerivedTypes<SeqArgVariable>(mArg2))
            {
                cmd += "_tv";
            }
            else
            {
                const SeqArg16* arg2DefaultType = sead::DynamicCast<SeqArg16>(mArg2);
                SEAD_ASSERT(arg2DefaultType);
                SEAD_ASSERT(arg2DefaultType->mHasSign);
                cmd += "_t";
            }
        }

        return toString_(cmd);
    }

    SeqArgBase* mArg1;
    SeqArgBase* mArg2;
};

enum class MmlExCommandArgType
{
    Invalid = 0,
    //U8,
    U16,
    Variable_S16,
    Num
};

class MmlExCommandBase : public MmlCommandBase
{
    MmlExCommandArgType getArgType_() const
    {
        switch (getCommand_() & 0xF0)
        {
            case 0x80:
            case 0x90:
                return MmlExCommandArgType::Variable_S16;

            case 0xE0:
                return MmlExCommandArgType::U16;

            // The following is only present in newer versions
            // case 0xA0:
            // case 0xB0:
            //     return MmlExCommandArgType::U8;

            default:
                return MmlExCommandArgType::Invalid;
        }
    }

public:
    MmlExCommandBase(SeqArgBase* arg1, SeqArgBase* arg2 = nullptr, bool conditional = false)
        : MmlCommandBase(conditional)
    {
        mArg1 = arg1;
        mArg2 = arg2;
    }

    void init() override
    {
        SeqArgBase* arg1 = mArg1;
        SeqArgBase* arg2 = mArg2;

        mArg1 = nullptr;
        mArg2 = nullptr;

        MmlExCommandArgType argType = getArgType_();
        SEAD_ASSERT(MmlExCommandArgType::Invalid < argType && argType < MmlExCommandArgType::Num);

        switch (argType)
        {
            //case MmlExCommandArgType::U8:
                // TODO

            case MmlExCommandArgType::U16:
            {
                const SeqArg16* arg1DefaultType = sead::DynamicCast<SeqArg16>(arg1);
                if (arg1DefaultType)
                {
                    SEAD_ASSERT(!arg1DefaultType->mHasSign);
                }
                else
                {
                    SEAD_ASSERT(sead::IsDerivedTypes<SeqArgRandom>(arg1) || sead::IsDerivedTypes<SeqArgVariable>(arg1));
                }

                mArg1 = arg1;
                break;
            }

            case MmlExCommandArgType::Variable_S16:
            {
                SEAD_ASSERT(sead::IsDerivedTypes<SeqArgVariable>(arg1));
                mArg1 = arg1;

                const SeqArg16* arg2DefaultType = sead::DynamicCast<SeqArg16>(arg2);
                if (arg2DefaultType)
                {
                    SEAD_ASSERT(arg2DefaultType->mHasSign);
                }
                else
                {
                    SEAD_ASSERT(sead::IsDerivedTypes<SeqArgRandom>(arg2) || sead::IsDerivedTypes<SeqArgVariable>(arg2));
                }

                mArg2 = arg2;
                break;
            }
        }
    }

    virtual MmlCommand::MmlEx getCommand_() const = 0;
    virtual const char* getCommandString_() const = 0;

    virtual std::vector<u8> encode()
    {
        std::vector<u8> ret = MmlCommandBase::encode();

        MmlExCommandArgType argType = getArgType_();
        SEAD_ASSERT(MmlExCommandArgType::Invalid < argType && argType < MmlExCommandArgType::Num);

        switch (argType)
        {
            //case MmlExCommandArgType::U8:
                // TODO

            case MmlExCommandArgType::U16:
                if (sead::IsDerivedTypes<SeqArgRandom>(mArg1))
                {
                    ret.push_back(MmlCommand::MML_RANDOM);
                }
                else if (sead::IsDerivedTypes<SeqArgVariable>(mArg1))
                {
                    ret.push_back(MmlCommand::MML_VARIABLE);
                }
                else
                {
                    const SeqArg16* arg1DefaultType = sead::DynamicCast<SeqArg16>(mArg1);
                    SEAD_ASSERT(arg1DefaultType);
                    SEAD_ASSERT(!arg1DefaultType->mHasSign);
                }
                break;

            case MmlExCommandArgType::Variable_S16:
                SEAD_ASSERT(sead::IsDerivedTypes<SeqArgVariable>(mArg1));

                if (sead::IsDerivedTypes<SeqArgRandom>(mArg2))
                {
                    ret.push_back(MmlCommand::MML_RANDOM);
                }
                else if (sead::IsDerivedTypes<SeqArgVariable>(mArg2))
                {
                    ret.push_back(MmlCommand::MML_VARIABLE);
                }
                else
                {
                    const SeqArg16* arg2DefaultType = sead::DynamicCast<SeqArg16>(mArg2);
                    SEAD_ASSERT(arg2DefaultType);
                    SEAD_ASSERT(arg2DefaultType->mHasSign);
                }
                break;
        }

        ret.push_back(MmlCommand::MML_EX_COMMAND);
        ret.push_back(getCommand_());

        switch (argType)
        {
            //case MmlExCommandArgType::U8:
                // TODO

            case MmlExCommandArgType::U16:
            {
                const auto& arg1Bytes = mArg1->encode();
                ret.insert(ret.end(), arg1Bytes.begin(), arg1Bytes.end());
                break;
            }

            case MmlExCommandArgType::Variable_S16:
            {
                const auto& arg1Bytes = mArg1->encode();
                ret.insert(ret.end(), arg1Bytes.begin(), arg1Bytes.end());

                const auto& arg2Bytes = mArg2->encode();
                ret.insert(ret.end(), arg2Bytes.begin(), arg2Bytes.end());
                break;
            }
        }

        return ret;
    }

    std::vector<std::string> getArgs_() const override
    {
        if (getArgType_() == MmlExCommandArgType::Variable_S16)
            return { mArg1->toString(), mArg2->toString() };

        return { mArg1->toString() };
    }

    std::string toString() const override
    {
        std::string cmd = getCommandString_();

        MmlExCommandArgType argType = getArgType_();
        SEAD_ASSERT(MmlExCommandArgType::Invalid < argType && argType < MmlExCommandArgType::Num);

        switch (argType)
        {
            //case MmlExCommandArgType::U8:
                // TODO

            case MmlExCommandArgType::U16:
                if (sead::IsDerivedTypes<SeqArgRandom>(mArg1))
                {
                    cmd += "_r";
                }
                else if (sead::IsDerivedTypes<SeqArgVariable>(mArg1))
                {
                    cmd += "_v";
                }
                else
                {
                    const SeqArg16* arg1DefaultType = sead::DynamicCast<SeqArg16>(mArg1);
                    SEAD_ASSERT(arg1DefaultType);
                    SEAD_ASSERT(!arg1DefaultType->mHasSign);
                }
                break;

            case MmlExCommandArgType::Variable_S16:
                SEAD_ASSERT(sead::IsDerivedTypes<SeqArgVariable>(mArg1));

                if (sead::IsDerivedTypes<SeqArgRandom>(mArg2))
                {
                    cmd += "_r";
                }
                else if (sead::IsDerivedTypes<SeqArgVariable>(mArg2))
                {
                    cmd += "_v";
                }
                else
                {
                    const SeqArg16* arg2DefaultType = sead::DynamicCast<SeqArg16>(mArg2);
                    SEAD_ASSERT(arg2DefaultType);
                    SEAD_ASSERT(arg2DefaultType->mHasSign);
                }
                break;
        }

        return toString_(cmd);
    }

    SeqArgBase* mArg1;
    SeqArgBase* mArg2;
};

class MmlCommandNote : public MmlCommandBase
{
public:
    static const u32 sKeysNum = 128;

    static const char* const sKeys[sKeysNum];

public:
    MmlCommandNote(u32 key, u8 velocity, SeqArgBase* length, bool conditional = false)
        : MmlCommandBase(conditional)
    {
        SEAD_ASSERT(0 <= key && key <= 127);
        SEAD_ASSERT(sead::IsDerivedTypes<SeqArgVMIDI>(length) || sead::IsDerivedTypes<SeqArgRandom>(length) || sead::IsDerivedTypes<SeqArgVariable>(length));

        mKey = (u8)key;
        mVelocity = velocity;
        mLength = length;
    }

    std::vector<u8> encode() override
    {
        std::vector<u8> ret = MmlCommandBase::encode();

        if (sead::IsDerivedTypes<SeqArgRandom>(mLength))
        {
            ret.push_back(MmlCommand::MML_RANDOM);
        }
        else if (sead::IsDerivedTypes<SeqArgVariable>(mLength))
        {
            ret.push_back(MmlCommand::MML_VARIABLE);
        }
        else
        {
            SEAD_ASSERT(sead::IsDerivedTypes<SeqArgVMIDI>(mLength));
        }

        ret.push_back(mKey);
        ret.push_back(mVelocity);

        const auto& lengthBytes = mLength->encode();
        ret.insert(ret.end(), lengthBytes.begin(), lengthBytes.end());

        return ret;
    }

    std::vector<std::string> getArgs_() const override
    {
        return { std::to_string(mVelocity), mLength->toString() };
    }

    std::string toString() const override
    {
        std::string cmd = sKeys[mKey];

        if (sead::IsDerivedTypes<SeqArgRandom>(mLength))
        {
            cmd += "_r";
        }
        else if (sead::IsDerivedTypes<SeqArgVariable>(mLength))
        {
            cmd += "_v";
        }
        else
        {
            SEAD_ASSERT(sead::IsDerivedTypes<SeqArgVMIDI>(mLength));
        }

        return toString_(cmd);
    }

    u8 mKey;
    u8 mVelocity;
    SeqArgBase* mLength;
};

class MmlCommandWait : public MmlCommandArgs1<SeqArgVMIDI>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArgVMIDI* arg) const override
    {
        return true;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_WAIT;
    }

    const char* getCommandString_() const override
    {
        return "wait";
    }
};

class MmlCommandFin : public MmlCommandArgs0
{
public:
    using MmlCommandArgs0::MmlCommandArgs0;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_FIN;
    }

    const char* getCommandString_() const override
    {
        return "fin";
    }
};

class MmlCommandPrg : public MmlCommandArgs1<SeqArgVMIDI>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArgVMIDI* arg) const override
    {
        return arg->mValue < 0x10000;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_PRG;
    }

    const char* getCommandString_() const override
    {
        return "prg";
    }
};

class MmlCommandTempo : public MmlCommandArgs1<SeqArg16>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg16* arg) const override
    {
        return arg->mHasSign && 0 <= arg->mValue && arg->mValue <= 1023;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_TEMPO;
    }

    const char* getCommandString_() const override
    {
        return "tempo";
    }
};

class MmlCommandTimebase : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_TIMEBASE;
    }

    const char* getCommandString_() const override
    {
        return "timebase";
    }
};

class MmlCommandVolumeBase : public MmlCommandArgs2<SeqArg8>
{
public:
    using MmlCommandArgs2::MmlCommandArgs2;

    bool validateArg1ValueDefaultType_(const SeqArg8* arg1) const override
    {
        return !arg1->mHasSign;
    }
};

class MmlCommandVolume : public MmlCommandVolumeBase
{
public:
    using MmlCommandVolumeBase::MmlCommandVolumeBase;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_VOLUME;
    }

    const char* getCommandString_() const override
    {
        return "volume";
    }
};

class MmlCommandVolume2 : public MmlCommandVolumeBase
{
public:
    using MmlCommandVolumeBase::MmlCommandVolumeBase;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_VOLUME2;
    }

    const char* getCommandString_() const override
    {
        return "volume2";
    }
};

class MmlCommandMainVolume : public MmlCommandVolumeBase
{
public:
    using MmlCommandVolumeBase::MmlCommandVolumeBase;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_MAIN_VOLUME;
    }

    const char* getCommandString_() const override
    {
        return "main_volume";
    }
};

class MmlCommandPitchBend : public MmlCommandArgs2<SeqArg8>
{
public:
    using MmlCommandArgs2::MmlCommandArgs2;

    bool validateArg1ValueDefaultType_(const SeqArg8* arg1) const override
    {
        return arg1->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_PITCH_BEND;
    }

    const char* getCommandString_() const override
    {
        return "pitchbend";
    }
};

class MmlCommandBendRange : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_BEND_RANGE;
    }

    const char* getCommandString_() const override
    {
        return "bendrange";
    }
};

class MmlCommandTranspose : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_TRANSPOSE;
    }

    const char* getCommandString_() const override
    {
        return "transpose";
    }
};

class MmlCommandVelocityRange : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_VELOCITY_RANGE;
    }

    const char* getCommandString_() const override
    {
        return "velocity_range";
    }
};

class MmlCommandPan : public MmlCommandArgs2<SeqArg8>
{
public:
    using MmlCommandArgs2::MmlCommandArgs2;

    bool validateArg1ValueDefaultType_(const SeqArg8* arg1) const override
    {
        return !arg1->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_PAN;
    }

    const char* getCommandString_() const override
    {
        return "pan";
    }
};

class MmlCommandSurroundPan : public MmlCommandArgs2<SeqArg8>
{
public:
    using MmlCommandArgs2::MmlCommandArgs2;

    bool validateArg1ValueDefaultType_(const SeqArg8* arg1) const override
    {
        return !arg1->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_SURROUND_PAN;
    }

    const char* getCommandString_() const override
    {
        return "span";
    }
};

class MmlCommandInitPan : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_INIT_PAN;
    }

    const char* getCommandString_() const override
    {
        return "init_pan";
    }
};

class MmlCommandPrio : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_PRIO;
    }

    const char* getCommandString_() const override
    {
        return "prio";
    }
};

class MmlCommandTie : public MmlCommandBoolArg
{
public:
    using MmlCommandBoolArg::MmlCommandBoolArg;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_TIE;
    }

    const char* getCommandString_() const override
    {
        return "tie";
    }
};

class MmlCommandMonophonic : public MmlCommandBoolArg
{
public:
    using MmlCommandBoolArg::MmlCommandBoolArg;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_MONOPHONIC;
    }

    const char* getCommandString_() const override
    {
        return "monophonic_";
    }
};

class MmlCommandNoteWait : public MmlCommandBoolArg
{
public:
    using MmlCommandBoolArg::MmlCommandBoolArg;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_NOTE_WAIT;
    }

    const char* getCommandString_() const override
    {
        return "notewait_";
    }
};

class MmlCommandDamper : public MmlCommandBoolArg
{
public:
    using MmlCommandBoolArg::MmlCommandBoolArg;

    bool validateArgValue_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    bool isTrue_(const SeqArg8* arg) const override
    {
        return arg->mValue >= 64;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_DAMPER;
    }

    const char* getCommandString_() const override
    {
        return "damper_";
    }
};

class MmlCommandPorta : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    std::vector<std::string> getArgs_() const override
    {
        const SeqArg8* argDefaultType = sead::DynamicCast<SeqArg8>(mArg);
        SEAD_ASSERT(argDefaultType);

        return { MmlCommandNote::sKeys[argDefaultType->mValue] };
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_PORTA;
    }

    const char* getCommandString_() const override
    {
        return "porta";
    }
};

class MmlCommandPortaTime : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_PORTA_TIME;
    }

    const char* getCommandString_() const override
    {
        return "porta_time";
    }
};

class MmlCommandPortaSw : public MmlCommandBoolArg
{
public:
    using MmlCommandBoolArg::MmlCommandBoolArg;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_PORTA_SW;
    }

    const char* getCommandString_() const override
    {
        return "porta_";
    }
};

class MmlCommandSweepPitch : public MmlCommandArgs1<SeqArg16>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg16* arg) const override
    {
        return arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_SWEEP_PITCH;
    }

    const char* getCommandString_() const override
    {
        return "sweep_pitch";
    }
};

class MmlCommandLPFCutoff : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_LPF_CUTOFF;
    }

    const char* getCommandString_() const override
    {
        return "lpf_cutoff";
    }
};

class MmlCommandBiquadType : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_BIQUAD_TYPE;
    }

    const char* getCommandString_() const override
    {
        return "biquad_type";
    }
};

class MmlCommandBiquadValue : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_BIQUAD_VALUE;
    }

    const char* getCommandString_() const override
    {
        return "biquad_value";
    }
};

class MmlCommandSendBase : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }
};

class MmlCommandFxSendA : public MmlCommandSendBase
{
public:
    using MmlCommandSendBase::MmlCommandSendBase;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_FXSEND_A;
    }

    const char* getCommandString_() const override
    {
        return "fxsend_a";
    }
};

class MmlCommandFxSendB : public MmlCommandSendBase
{
public:
    using MmlCommandSendBase::MmlCommandSendBase;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_FXSEND_B;
    }

    const char* getCommandString_() const override
    {
        return "fxsend_b";
    }
};

class MmlCommandFxSendC : public MmlCommandSendBase
{
public:
    using MmlCommandSendBase::MmlCommandSendBase;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_FXSEND_C;
    }

    const char* getCommandString_() const override
    {
        return "fxsend_c";
    }
};

class MmlCommandMainSend : public MmlCommandSendBase
{
public:
    using MmlCommandSendBase::MmlCommandSendBase;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_MAINSEND;
    }

    const char* getCommandString_() const override
    {
        return "mainsend";
    }
};

class MmlCommandAllocTrack : public MmlCommandBase
{
public:
    MmlCommandAllocTrack(SeqArgBase* arg)
        : MmlCommandBase(false)
    {
        SEAD_ASSERT(validateArg_(arg));
        mArg = sead::DynamicCast<SeqArg16>(arg);
    }

    bool validateArg_(const SeqArgBase* arg) const
    {
        const SeqArg16* argDefaultType = sead::DynamicCast<const SeqArg16>(arg);
        return argDefaultType && !argDefaultType->mHasSign;
    }

    std::vector<u8> encode() override
    {
        SEAD_ASSERT(validateArg_(mArg));

        std::vector<u8> ret = MmlCommandBase::encode();

        ret.push_back(MmlCommand::MML_ALLOC_TRACK);

        const auto& argBytes = mArg->encode();
        ret.insert(ret.end(), argBytes.begin(), argBytes.end());

        return ret;
    }

    std::vector<std::string> getArgs_() const override
    {
        std::string ret;

        std::vector<std::string> args;
        for (u32 i = 0; i < 16; i++)
        {
            if ((mArg->mValue >> i) & 1)
                args.push_back("TRACK_" + std::to_string(i));
        }

        for (u32 i = 0; i < args.size(); i++)
        {
            ret += args[i];

            if (i < args.size() - 1)
                ret += " | ";
        }

        return { ret };
    }

    std::string toString() const override
    {
        SEAD_ASSERT(validateArg_(mArg));

        return toString_("alloctrack");
    }

    SeqArg16* mArg;
};

struct LabelObj
{
    LabelObj()
    {
        mOffset = 0;
        mLabel = nullptr;
    }

    LabelObj(u32 offset, const char* label)
    {
        SEAD_ASSERT(offset <= 0xFFFFFF);
        SEAD_ASSERT(label);

        mOffset = offset;
        mLabel = label;
    }

    std::vector<u8> encode()
    {
        SEAD_ASSERT(mOffset <= 0xFFFFFF);

        return { (u8)(mOffset >> 16), (u8)(mOffset >> 8), (u8)mOffset };
    }

    u32 mOffset;
    const char* mLabel;
};

class MmlCommandOpenTrack : public MmlCommandBase
{
public:
    MmlCommandOpenTrack(u8 trackNo, LabelObj labelObj, bool conditional = false)
        : MmlCommandBase(conditional)
    {
        SEAD_ASSERT(0 <= trackNo && trackNo < 16);

        mTrackNo = trackNo;
        mLabelObj = labelObj;
    }

    std::vector<u8> encode() override
    {
        SEAD_ASSERT(0 <= mTrackNo && mTrackNo < 16);

        std::vector<u8> ret = MmlCommandBase::encode();

        ret.push_back(MmlCommand::MML_OPEN_TRACK);
        ret.push_back(mTrackNo);

        const auto& labelObjBytes = mLabelObj.encode();
        ret.insert(ret.end(), labelObjBytes.begin(), labelObjBytes.end());

        return ret;
    }

    std::vector<std::string> getArgs_() const override
    {
        return { std::to_string(mTrackNo), mLabelObj.mLabel };
    }

    std::string toString() const override
    {
        SEAD_ASSERT(0 <= mTrackNo && mTrackNo < 16);

        return toString_("opentrack");
    }

    u8 mTrackNo;
    LabelObj mLabelObj;
};

class MmlCommandJump : public MmlCommandBase
{
public:
    MmlCommandJump(LabelObj labelObj, bool conditional = false)
        : MmlCommandBase(conditional)
    {
        mLabelObj = labelObj;
    }

    std::vector<u8> encode() override
    {
        std::vector<u8> ret = MmlCommandBase::encode();

        ret.push_back(MmlCommand::MML_JUMP);

        const auto& labelObjBytes = mLabelObj.encode();
        ret.insert(ret.end(), labelObjBytes.begin(), labelObjBytes.end());

        return ret;
    }

    std::vector<std::string> getArgs_() const override
    {
        return { mLabelObj.mLabel };
    }

    std::string toString() const override
    {
        return toString_("jump");
    }

    LabelObj mLabelObj;
};

class MmlCommandCall : public MmlCommandBase
{
public:
    MmlCommandCall(LabelObj labelObj, bool conditional = false)
        : MmlCommandBase(conditional)
    {
        mLabelObj = labelObj;
    }

    std::vector<u8> encode() override
    {
        std::vector<u8> ret = MmlCommandBase::encode();

        ret.push_back(MmlCommand::MML_CALL);

        const auto& labelObjBytes = mLabelObj.encode();
        ret.insert(ret.end(), labelObjBytes.begin(), labelObjBytes.end());

        return ret;
    }

    std::vector<std::string> getArgs_() const override
    {
        return { mLabelObj.mLabel };
    }

    std::string toString() const override
    {
        return toString_("call");
    }

    LabelObj mLabelObj;
};

class MmlCommandRet : public MmlCommandArgs0
{
public:
    using MmlCommandArgs0::MmlCommandArgs0;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_RET;
    }

    const char* getCommandString_() const override
    {
        return "ret";
    }
};

class MmlCommandLoopStart : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_LOOP_START;
    }

    const char* getCommandString_() const override
    {
        return "loop_start";
    }
};

class MmlCommandLoopEnd : public MmlCommandArgs0
{
public:
    using MmlCommandArgs0::MmlCommandArgs0;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_LOOP_END;
    }

    const char* getCommandString_() const override
    {
        return "loop_end";
    }
};

class MmlCommandAttack : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return true;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_ATTACK;
    }

    const char* getCommandString_() const override
    {
        return "attack";
    }
};

class MmlCommandEnvHold : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return true;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_ENV_HOLD;
    }

    const char* getCommandString_() const override
    {
        return "env_hold";
    }
};

class MmlCommandDecay : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return true;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_DECAY;
    }

    const char* getCommandString_() const override
    {
        return "decay";
    }
};

class MmlCommandSustain : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return true;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_SUSTAIN;
    }

    const char* getCommandString_() const override
    {
        return "sustain";
    }
};

class MmlCommandRelease : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return true;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_RELEASE;
    }

    const char* getCommandString_() const override
    {
        return "release";
    }
};

class MmlCommandEnvReset : public MmlCommandArgs0
{
public:
    using MmlCommandArgs0::MmlCommandArgs0;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_ENV_RESET;
    }

    const char* getCommandString_() const override
    {
        return "env_reset";
    }
};

class MmlCommandModDepth : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_MOD_DEPTH;
    }

    const char* getCommandString_() const override
    {
        return "mod_depth";
    }
};

class MmlCommandModRange : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_MOD_RANGE;
    }

    const char* getCommandString_() const override
    {
        return "mod_range";
    }
};

class MmlCommandModSpeed : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_MOD_SPEED;
    }

    const char* getCommandString_() const override
    {
        return "mod_speed";
    }
};

class MmlCommandModDelay : public MmlCommandArgs1<SeqArg16>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg16* arg) const override
    {
        return arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_MOD_DELAY;
    }

    const char* getCommandString_() const override
    {
        return "mod_delay";
    }
};

class MmlCommandModType : public MmlCommandArgs1<SeqArg8>
{
public:
    static const char* const sTypes[];

public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return 0 <= arg->mValue && arg->mValue <= 2;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_MOD_TYPE;
    }

    std::vector<std::string> getArgs_() const override
    {
        const SeqArg8* argDefaultType = sead::DynamicCast<const SeqArg8>(mArg);
        if (argDefaultType && 0 <= argDefaultType->mValue && argDefaultType->mValue <= 2)
            return { sTypes[argDefaultType->mValue] };

        return { mArg->toString() };
    }

    const char* getCommandString_() const override
    {
        return "mod_type";
    }
};

class MmlCommandModPeriod : public MmlCommandArgs1<SeqArg16>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg16* arg) const override
    {
        return arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_MOD_PERIOD;
    }

    const char* getCommandString_() const override
    {
        return "mod_period";
    }
};

class MmlCommandMute : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return 0 <= arg->mValue && arg->mValue <= 3;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_MUTE;
    }

    const char* getCommandString_() const override
    {
        return "mute";
    }
};

class MmlCommandUserproc : public MmlExCommandBase
{
public:
    MmlCommandUserproc(SeqArgBase* arg, bool conditional = false)
        : MmlExCommandBase(arg, nullptr, conditional)
    {
    }

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_USERPROC;
    }

    const char* getCommandString_() const override
    {
        return "userproc";
    }
};

class MmlCommandBankSelect : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return !arg->mHasSign;
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_BANK_SELECT;
    }

    const char* getCommandString_() const override
    {
        return "bank_select";
    }
};

class MmlCommandSetVar : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_SETVAR;
    }

    const char* getCommandString_() const override
    {
        return "setvar";
    }
};

class MmlCommandAddVar : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_ADDVAR;
    }

    const char* getCommandString_() const override
    {
        return "addvar";
    }
};

class MmlCommandSubVar : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_SUBVAR;
    }

    const char* getCommandString_() const override
    {
        return "subvar";
    }
};

class MmlCommandMulVar : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_MULVAR;
    }

    const char* getCommandString_() const override
    {
        return "mulvar";
    }
};

class MmlCommandDivVar : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_DIVVAR;
    }

    const char* getCommandString_() const override
    {
        return "divvar";
    }
};

class MmlCommandShiftVar : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_SHIFTVAR;
    }

    const char* getCommandString_() const override
    {
        return "shiftvar";
    }
};

class MmlCommandRandVar : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_RANDVAR;
    }

    const char* getCommandString_() const override
    {
        return "randvar";
    }
};

class MmlCommandAndVar : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_ANDVAR;
    }

    const char* getCommandString_() const override
    {
        return "andvar";
    }
};

class MmlCommandOrVar : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_ORVAR;
    }

    const char* getCommandString_() const override
    {
        return "orvar";
    }
};

class MmlCommandXorVar : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_XORVAR;
    }

    const char* getCommandString_() const override
    {
        return "xorvar";
    }
};

class MmlCommandNotVar : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_NOTVAR;
    }

    const char* getCommandString_() const override
    {
        return "notvar";
    }
};

class MmlCommandModVar : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_MODVAR;
    }

    const char* getCommandString_() const override
    {
        return "modvar";
    }
};

class MmlCommandPrintVar : public MmlCommandArgs1<SeqArg8>
{
public:
    using MmlCommandArgs1::MmlCommandArgs1;

    bool validateArgValueDefaultType_(const SeqArg8* arg) const override
    {
        return 0 <= arg->mValue && arg->mValue < 16 * 3;
    }

    std::vector<std::string> getArgs_() const override
    {
        const SeqArg8* argDefaultType = sead::DynamicCast<const SeqArg8>(mArg);
        if (!argDefaultType)
        {
            return MmlCommandArgs1::getArgs_();
        }

        SEAD_ASSERT(0 <= argDefaultType->mValue && argDefaultType->mValue < 16 * 3);

        switch (argDefaultType->mValue / 16)
        {
            case 0:
                return { std::format("VAR_{:d}", (u32)argDefaultType->mValue % 16) };
            case 1:
                return { std::format("GVAR_{:d}", (u32)argDefaultType->mValue % 16) };
            case 2:
                return { std::format("TVAR_{:d}", (u32)argDefaultType->mValue % 16) };
        }

        return { "(Invalid)" };
    }

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_PRINTVAR;
    }

    const char* getCommandString_() const override
    {
        return "printvar";
    }
};

class MmlCommandCmpEq : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_CMP_EQ;
    }

    const char* getCommandString_() const override
    {
        return "cmp_eq";
    }
};

class MmlCommandCmpGe : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_CMP_GE;
    }

    const char* getCommandString_() const override
    {
        return "cmp_ge";
    }
};

class MmlCommandCmpGt : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_CMP_GT;
    }

    const char* getCommandString_() const override
    {
        return "cmp_gt";
    }
};

class MmlCommandCmpLe : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_CMP_LE;
    }

    const char* getCommandString_() const override
    {
        return "cmp_le";
    }
};

class MmlCommandCmpLt : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_CMP_LT;
    }

    const char* getCommandString_() const override
    {
        return "cmp_lt";
    }
};

class MmlCommandCmpNe : public MmlExCommandBase
{
public:
    using MmlExCommandBase::MmlExCommandBase;

    MmlCommand::MmlEx getCommand_() const override
    {
        return MmlCommand::MML_CMP_NE;
    }

    const char* getCommandString_() const override
    {
        return "cmp_ne";
    }
};

class MmlCommandFrontBypass : public MmlCommandBoolArg
{
public:
    using MmlCommandBoolArg::MmlCommandBoolArg;

    MmlCommand::Mml getCommand_() const override
    {
        return MmlCommand::MML_FRONT_BYPASS;
    }

    const char* getCommandString_() const override
    {
        return "frontbypass_";
    }
};
