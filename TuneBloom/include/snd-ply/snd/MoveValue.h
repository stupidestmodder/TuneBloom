#pragma once

namespace snd { namespace internal {

template <typename ValueType, typename CountType>
class MoveValue
{
public:
    MoveValue()
        : mOrigin(0)
        , mTarget(0)
        , mFrame(0)
        , mCounter(0)
    {
    }

    void initValue(ValueType value)
    {
        mOrigin = value;
        mTarget = value;
        mFrame = 0;
        mCounter = 0;
    }

    void setTarget(ValueType targetValue, CountType frames)
    {
        mOrigin = this->getValue();
        mTarget = targetValue;
        mFrame = frames;
        mCounter = 0;
    }

    ValueType getTarget() const
    {
        return mTarget;
    }

    ValueType getValue() const
    {
        if (this->isFinished())
            return mTarget;

        return static_cast<ValueType>(mOrigin + ( mTarget - mOrigin ) * mCounter / mFrame);
    }

    void update()
    {
        if (mCounter < mFrame)
            mCounter++;
    }

    bool isFinished() const
    {
        return mCounter >= mFrame;
    }

    CountType getRemainingCount() const
    {
        if (this->isFinished())
            return 0;

        return mFrame - mCounter;
    }

private:
    ValueType mOrigin;
    ValueType mTarget;
    CountType mFrame;
    CountType mCounter;
};

} } // namespace snd::internal
