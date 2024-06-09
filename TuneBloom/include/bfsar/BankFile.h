#pragma once

#include <bfsar/WaveFile.h>

#include <snd/snd_BankFileReader.h>

#include <unordered_map>

class BankFile : public Item
{
public:
    class VelocityRegion : public Item
    {
    public:
        VelocityRegion(u8 keyMin, u8 keyMax)
            : Item()
            , mKeyMin(keyMin)
            , mKeyMax(keyMax)

            , mWaveFileRef(this)
            , mOriginalKey(60)
            , mVolume(127)
            , mPan(64)
            , mPitch(1.0f)
            , mIsIgnoreNoteOff(false)
            , mKeyGroup(0)
            , mInterpolationType(0)
            , mAdshrCurve()
        {
            mItemType = ItemType::BankFileVelocityRegion;

            SEAD_ASSERT(mKeyMin <= 127);
            SEAD_ASSERT(mKeyMax <= 127);
        }

        void read(const nw::snd::internal::BankFile::VelocityRegion* velocityRegionInfo, const nw::snd::internal::Util::WaveIdTable& waveIdTable);

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

        u8 getVolume() const
        {
            return mVolume;
        }

        u8 getPan() const
        {
            return mPan;
        }

        f32 getPitch() const
        {
            return mPitch;
        }

        bool isIgnoreNoteOff() const
        {
            return mIsIgnoreNoteOff;
        }

        u8 getKeyGroup() const
        {
            return mKeyGroup;
        }

        u8 getInterpolationType() const
        {
            return mInterpolationType;
        }

        const snd::AdshrCurve& getAdshrCurve() const
        {
            return mAdshrCurve;
        }

    private:
        u8 mKeyMin;
        u8 mKeyMax;

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

        void read(const nw::snd::internal::BankFile::KeyRegion* keyRegionInfo, const nw::snd::internal::Util::WaveIdTable& waveIdTable);

        const VelocityRegion* getVelocityRegion(u8 velocity) const
        {
            for (const Item* item : mVelocityRegionList)
            {
                SEAD_ASSERT(item->getItemType() == ItemType::BankFileVelocityRegion);
                const VelocityRegion* velocityRegion = static_cast<const VelocityRegion*>(item);

                if (velocityRegion->mKeyMin <= velocity && velocity <= velocityRegion->mKeyMax)
                {
                    return velocityRegion;
                }
            }

            return nullptr;
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

    private:
        u32 mProgramNo;
        KeyRegion::List mKeyRegionList;

        friend class BankFile;
    };

public:
    BankFile()
        : Item()
        , mInstrumentList()
    {
        mItemType = ItemType::BankFile;
    }

    void read(const void* bankFile);
    void drawUI();

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

private:
    Instrument::List mInstrumentList;
};
