#pragma once

#include <bfsar/Item.h>
#include <bfsar/InnerFile.h>
#include <bfsar/WaveFile.h>

#include <snd/snd_BankFileReader.h>

#include <unordered_map>

class Bank;
class WaveArchive;

class BankFile : public Item, public InnerFile
{
    SEAD_RTTI_OVERRIDE(BankFile, InnerFile);

public:
    class Instrument;
    class KeyRegion;

    class VelocityRegion : public Item
    {
    public:
        VelocityRegion(u8 velocityMin, u8 velocityMax)
            : Item()
            , mVelocityMin(velocityMin)
            , mVelocityMax(velocityMax)

            , mWaveFileRef(this)
            , mOriginalKey(60)
            , mVolume(127)
            , mPan(64)
            , mPitch(1.0f)
            , mIsIgnoreNoteOff(false)
            , mKeyGroup(0)
            , mInterpolationType(0)
            , mAdshrCurve(127, 127, 127, 0, 127)
        {
            mItemType = ItemType::BankFileVelocityRegion;

            SEAD_ASSERT(mVelocityMin <= 127);
            SEAD_ASSERT(mVelocityMax <= 127);
        }

        void read(const nw::snd::internal::BankFile::VelocityRegion* velocityRegionInfo, const nw::snd::internal::Util::WaveIdTable& waveIdTable, u32 instrumentId);
        void drawUI();

        u8 getVelocityMin() const
        {
            return mVelocityMin;
        }

        void setVelocityMin(u8 velocityMin, const KeyRegion& parentRegion);

        u8 getVelocityMax() const
        {
            return mVelocityMax;
        }

        void setVelocityMax(u8 velocityMax, const KeyRegion& parentRegion);

        VelocityRegion* getPrev(const KeyRegion& parentRegion);
        VelocityRegion* getNext(const KeyRegion& parentRegion);

        u8 getVelocityNum() const
        {
            return mVelocityMax - mVelocityMin + 1;
        }

        const ItemReference& getWaveFileRef() const
        {
            return mWaveFileRef;
        }

        ItemReference& getWaveFileRef()
        {
            return mWaveFileRef;
        }

        u8 getOriginalKey() const
        {
            return mOriginalKey;
        }

        void setOriginalKey(u8 originalKey)
        {
            originalKey = sead::MathCalcCommon<u8>::clampMax(originalKey, 127);
            mOriginalKey = originalKey;
        }

        u8 getVolume() const
        {
            return mVolume;
        }

        void setVolume(u8 volume)
        {
            mVolume = volume;
        }

        u8 getPan() const
        {
            return mPan;
        }

        void setPan(u8 pan)
        {
            pan = sead::MathCalcCommon<u8>::clampMax(pan, 127);
            mPan = pan;
        }

        f32 getPitch() const
        {
            return mPitch;
        }

        void setPitch(f32 pitch)
        {
            pitch = sead::Mathf::clamp2(0.0f, pitch, 400.0f);
            mPitch = pitch;
        }

        bool getIsIgnoreNoteOff() const
        {
            return mIsIgnoreNoteOff;
        }

        void setIsIgnoreNoteOff(bool ignoreNoteOff)
        {
            mIsIgnoreNoteOff = ignoreNoteOff;
        }

        u8 getKeyGroup() const
        {
            return mKeyGroup;
        }

        void setKeyGroup(u8 keyGroup)
        {
            keyGroup = sead::MathCalcCommon<u8>::clampMax(keyGroup, 16 - 1);
            mKeyGroup = keyGroup;
        }

        u8 getInterpolationType() const
        {
            return mInterpolationType;
        }

        void setInterpolationType(u8 type)
        {
            type = sead::MathCalcCommon<u8>::clampMax(type, 2 - 1);
            mInterpolationType = type;
        }

        const snd::AdshrCurve& getAdshrCurve() const
        {
            return mAdshrCurve;
        }

        void setAdshrCurve(const snd::AdshrCurve& curve)
        {
            mAdshrCurve = curve;

            mAdshrCurve.attack = sead::MathCalcCommon<u8>::clampMax(mAdshrCurve.attack, 127);
            mAdshrCurve.decay = sead::MathCalcCommon<u8>::clampMax(mAdshrCurve.decay, 127);
            mAdshrCurve.sustain = sead::MathCalcCommon<u8>::clampMax(mAdshrCurve.sustain, 127);
            mAdshrCurve.hold = sead::MathCalcCommon<u8>::clampMax(mAdshrCurve.hold, 127);
            mAdshrCurve.release = sead::MathCalcCommon<u8>::clampMax(mAdshrCurve.release, 127);
        }

    private:
        u8 mVelocityMin;
        u8 mVelocityMax;

        ItemReference mWaveFileRef;
        u8 mOriginalKey;
        u8 mVolume;
        u8 mPan;
        f32 mPitch;
        bool mIsIgnoreNoteOff; // Percussion Mode
        u8 mKeyGroup;
        u8 mInterpolationType;
        snd::AdshrCurve mAdshrCurve;

        friend class BankFile;
    };

    class KeyRegion : public Item
    {
    public:
        KeyRegion(u8 keyMin, u8 keyMax)
            : Item()
            , mKeyMin(keyMin)
            , mKeyMax(keyMax)
            , mVelocityRegionList()
        {
            mItemType = ItemType::BankFileKeyRegion;

            SEAD_ASSERT(mKeyMin <= 127);
            SEAD_ASSERT(mKeyMax <= 127);
        }

        void read(const nw::snd::internal::BankFile::KeyRegion* keyRegionInfo, const nw::snd::internal::Util::WaveIdTable& waveIdTable, u32 instrumentId);

        const VelocityRegion* getVelocityRegion(u8 velocity) const
        {
            for (const Item* item : mVelocityRegionList)
            {
                SEAD_ASSERT(item->getItemType() == ItemType::BankFileVelocityRegion);
                const VelocityRegion* velocityRegion = static_cast<const VelocityRegion*>(item);

                if (velocityRegion->mVelocityMin <= velocity && velocity <= velocityRegion->mVelocityMax)
                {
                    return velocityRegion;
                }
            }

            return nullptr;
        }

        u8 getKeyMin() const
        {
            return mKeyMin;
        }

        void setKeyMin(u8 keyMin, const Instrument& parentInstrument);

        u8 getKeyMax() const
        {
            return mKeyMax;
        }

        void setKeyMax(u8 keyMax, const Instrument& parentInstrument);

        KeyRegion* getPrev(const Instrument& parentInstrument);
        KeyRegion* getNext(const Instrument& parentInstrument);
        KeyRegion* getPrevNeighbor(const Instrument& parentInstrument);
        KeyRegion* getNextNeighbor(const Instrument& parentInstrument);

        u8 getKeyNum() const
        {
            return mKeyMax - mKeyMin + 1;
        }

        const VelocityRegion::List& getVelocityRegionList() const
        {
            return mVelocityRegionList;
        }

        VelocityRegion::List& getVelocityRegionList()
        {
            return mVelocityRegionList;
        }

    private:
        u8 mKeyMin;
        u8 mKeyMax;
        VelocityRegion::List mVelocityRegionList;

        friend class BankFile;
    };

    class Instrument : public Item
    {
    public:
        Instrument()
            : Item()
            , mProgramNo(0)
            , mKeyRegionList()
        {
            mItemType = ItemType::BankFileInstrument;
        }

        void read(const nw::snd::internal::BankFile::Instrument* instrumentInfo, const nw::snd::internal::Util::WaveIdTable& waveIdTable);
        void drawUI();

        s16 getProgramNo() const
        {
            return mProgramNo;
        }

        void setProgramNo(s16 programNo)
        {
            programNo = sead::MathCalcCommon<s16>::clamp2(0, programNo, 32766);
            mProgramNo = programNo;
        }

        const KeyRegion* getKeyRegion(u8 key) const
        {
            for (const Item* item : mKeyRegionList)
            {
                SEAD_ASSERT(item->getItemType() == ItemType::BankFileKeyRegion);
                const KeyRegion* keyRegion = static_cast<const KeyRegion*>(item);

                if (keyRegion->mKeyMin <= key && key <= keyRegion->mKeyMax)
                {
                    return keyRegion;
                }
            }

            return nullptr;
        }

        const VelocityRegion* getVelocityRegion(u8 key, u8 velocity) const
        {
            const KeyRegion* keyRegion = getKeyRegion(key);
            if (!keyRegion)
            {
                return nullptr;
            }

            return keyRegion->getVelocityRegion(velocity);
        }

        const KeyRegion::List& getKeyRegionList() const
        {
            return mKeyRegionList;
        }

        KeyRegion::List& getKeyRegionList()
        {
            return mKeyRegionList;
        }

    private:
        s16 mProgramNo;
        KeyRegion::List mKeyRegionList;

        friend class BankFile;
    };

public:
    BankFile()
        : Item()
        , InnerFile()
        , mInstrumentList()

        , mBank(nullptr)
        , mWaveArchive(nullptr)
        , mWaveArchiveWaveFilesIndexes(nullptr)
        , mUpdateWriteInfo(true)
    {
        mItemType = ItemType::BankFile;
    }

    ~BankFile() override;

    const Item* validate(sead::BufferedSafeString& error) const override;

    void drawUI() override;
    void drawFileUI();

    void setup(sead::Endian::Types endian, u32 version) const
    {
        mEndian = endian;
        mVersion = version;
    }

    void prepare(const Bank* bank, const WaveArchive* warc, const std::unordered_map<const WaveArchive*, std::unordered_map<const WaveFile*, u32>>& waveFilesIndexes, bool updateWriteInfo) const
    {
        SEAD_ASSERT(!mBank);
        mBank = bank;

        SEAD_ASSERT(!mWaveArchive);
        mWaveArchive = warc;

        SEAD_ASSERT(!mWaveArchiveWaveFilesIndexes);

        const auto& it = waveFilesIndexes.find(warc);
        if (it != waveFilesIndexes.end())
        {
            mWaveArchiveWaveFilesIndexes = &it->second;
        }

        mUpdateWriteInfo = updateWriteInfo;
    }

private:
    bool doRead(const void* fileAddr) override;
    u32 doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const override;

    bool updateWriteInfo_() const override
    {
        return mUpdateWriteInfo;
    }

public:
    const Instrument* getInstrument(u8 programNo) const
    {
        for (const Item* item : mInstrumentList)
        {
            SEAD_ASSERT(item->getItemType() == ItemType::BankFileInstrument);
            const Instrument* instrument = static_cast<const Instrument*>(item);

            if (instrument->mProgramNo == programNo)
            {
                return instrument;
            }
        }

        return nullptr;
    }

    const Instrument::List& getInstrumentList() const
    {
        return mInstrumentList;
    }

    Instrument::List& getInstrumentList()
    {
        return mInstrumentList;
    }

private:
    Instrument::List mInstrumentList;

    mutable const Bank* mBank;
    mutable const WaveArchive* mWaveArchive;
    mutable const std::unordered_map<const WaveFile*, u32>* mWaveArchiveWaveFilesIndexes;
    mutable bool mUpdateWriteInfo;
};

inline void BankFile::KeyRegion::setKeyMin(u8 keyMin, const Instrument& parentInstrument)
{
    if (keyMin > 127)
    {
        keyMin = 127;
    }

    if (keyMin > mKeyMax)
    {
        mKeyMin = mKeyMax;
        return;
    }

    u8 min = 0;
    const KeyRegion* prev = getPrev(parentInstrument);
    if (prev)
    {
        min = prev->getKeyMax() + 1;
    }

    mKeyMin = sead::MathCalcCommon<u8>::clampMin(keyMin, min);
}

inline void BankFile::KeyRegion::setKeyMax(u8 keyMax, const Instrument& parentInstrument)
{
    if (keyMax > 127)
    {
        keyMax = 127;
    }

    if (keyMax < mKeyMin)
    {
        mKeyMax = mKeyMin;
        return;
    }

    u8 max = 127;
    const KeyRegion* next = getNext(parentInstrument);
    if (next)
    {
        max = next->getKeyMin() - 1;
    }

    mKeyMax = sead::MathCalcCommon<u8>::clampMax(keyMax, max);
}

inline BankFile::KeyRegion* BankFile::KeyRegion::getPrev(const Instrument& parentInstrument)
{
    auto* node = parentInstrument.getKeyRegionList().prev(this);
    if (node)
    {
        return static_cast<KeyRegion*>(node->val());
    }

    return nullptr;
}

inline BankFile::KeyRegion* BankFile::KeyRegion::getNext(const Instrument& parentInstrument)
{
    auto* node = parentInstrument.getKeyRegionList().next(this);
    if (node)
    {
        return static_cast<KeyRegion*>(node->val());
    }

    return nullptr;
}

inline BankFile::KeyRegion* BankFile::KeyRegion::getPrevNeighbor(const Instrument& parentInstrument)
{
    return const_cast<BankFile::KeyRegion*>(parentInstrument.getKeyRegion(getKeyMin() - 1));
}

inline BankFile::KeyRegion* BankFile::KeyRegion::getNextNeighbor(const Instrument& parentInstrument)
{
    return const_cast<BankFile::KeyRegion*>(parentInstrument.getKeyRegion(getKeyMax() + 1));
}

inline void BankFile::VelocityRegion::setVelocityMin(u8 velocityMin, const KeyRegion& parentRegion)
{
    if (velocityMin > 127)
    {
        velocityMin = 127;
    }

    // if (velocityMin > mVelocityMax)
    // {
    //     mVelocityMin = mVelocityMax;
    //     return;
    // }

    // u8 min = 0;
    // const VelocityRegion* prev = getPrev(parentRegion);
    // if (prev)
    // {
    //     min = prev->getVelocityMax() + 1;
    // }
    // else
    // {
    //     velocityMin = 0;
    // }

    // mVelocityMin = sead::MathCalcCommon<u8>::clampMin(velocityMin, min);
    mVelocityMin = velocityMin;
}

inline void BankFile::VelocityRegion::setVelocityMax(u8 velocityMax, const KeyRegion& parentRegion)
{
    if (velocityMax > 127)
    {
        velocityMax = 127;
    }

    // if (velocityMax < mVelocityMin)
    // {
    //     mVelocityMax = mVelocityMin;
    //     return;
    // }

    // u8 max = 127;
    // const VelocityRegion* next = getNext(parentRegion);
    // if (next)
    // {
    //     max = next->getVelocityMin() - 1;
    // }
    // else
    // {
    //     velocityMax = 127;
    // }

    // mVelocityMax = sead::MathCalcCommon<u8>::clampMax(velocityMax, max);
    mVelocityMax = velocityMax;
}

inline BankFile::VelocityRegion* BankFile::VelocityRegion::getPrev(const KeyRegion& parentRegion)
{
    auto* node = parentRegion.getVelocityRegionList().prev(this);
    if (node)
    {
        return static_cast<VelocityRegion*>(node->val());
    }

    return nullptr;
}

inline BankFile::VelocityRegion* BankFile::VelocityRegion::getNext(const KeyRegion& parentRegion)
{
    auto* node = parentRegion.getVelocityRegionList().next(this);
    if (node)
    {
        return static_cast<VelocityRegion*>(node->val());
    }

    return nullptr;
}
