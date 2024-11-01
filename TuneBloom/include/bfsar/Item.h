#pragma once

#include <container/seadTList.h>
#include <math/seadMathCalcCommon.h>
#include <prim/seadDelegate.h>
#include <prim/seadSafeString.h>

#include <snd/snd_SoundArchive.h>

template <typename T>
class List : public sead::TList<T>
{
public:
    List()
        : sead::TList<T>()
    {
    }

    ~List()
    {
        clear();
    }

    void clear()
    {
        for (auto it = this->robustBegin(); it != this->robustEnd(); ++it)
        {
            T item = (*it).val();

            this->erase(item);

            delete item;
        }

        sead::TList<T>::clear();
    }
};

enum class WaveArchiveType
{
    Invalid = -1,
    AutomaticShared,
    AutomaticIndividual,
    Explicit
};

class Item;

using ItemList = List<Item*>;

class ItemReference : public sead::TListNode<ItemReference*>
{
public:
    using ListNode = sead::TListNode<ItemReference*>;
    using List = sead::TList<ItemReference*>;

public:
    ItemReference(Item* owner)
        : ListNode(this)
        , mOwner(owner)
        , mItem(nullptr)
        , mDetachCallback()
    {
        SEAD_ASSERT(mOwner);
    }

    ~ItemReference()
    {
        detach();
    }

    void attach(Item* item);

    void detach()
    {
        mDetachCallback.invoke(mItem);

        mItem = nullptr;

        erase();
    }

    bool isAttached() const
    {
        return mItem != nullptr;
    }

    const Item* getOwner() const
    {
        return mOwner;
    }

    Item* getOwner()
    {
        return mOwner;
    }

    const Item* getItem() const
    {
        return mItem;
    }

    Item* getItem()
    {
        return mItem;
    }

    u32 getItemId() const;

    template <typename T>
    void setOnDetachCallback(sead::Delegate1<T, Item*>::MethodPtr method)
    {
        mDetachCallback.bind(static_cast<T*>(mOwner), method);
    }

private:
    Item* mOwner;
    Item* mItem;
    sead::AnyDelegate1<Item*> mDetachCallback;
};

class Item : public sead::TListNode<Item*>
{
public:
    enum class ItemType
    {
        Invalid = 0,

        Sound,
        SoundSet,
        Bank,
        WaveArchive,
        Group,
        Player,

        InternalFileTypes,

        WaveFile = InternalFileTypes,

        SequenceFile,

        BankFile,
        BankFileInstrument,
        BankFileKeyRegion,
        BankFileVelocityRegion,

        GroupItemInfo,

        StreamTrack
    };

    static const u32 cInvalidId = nw::snd::SoundArchive::INVALID_ID;
    static const u32 cMaxId = 0x00FFFFFF;

    using ListNode = sead::TListNode<Item*>;
    using List = ItemList;

public:
    Item()
        : ListNode(this)
        , mItemType(ItemType::Invalid)
        , mId(cInvalidId)
        , mEnableName(false)
        , mName()

        , mReferences()
    {
    }

    virtual ~Item()
    {
        for (ItemReference::List::robustIterator it = mReferences.robustBegin(); it != mReferences.robustEnd(); ++it)
        {
            ItemReference* ref = it->val();
            SEAD_ASSERT(ref->getItem() == this);

            ref->detach();
        }

        erase();
    }

    ItemType getItemType() const
    {
        return mItemType;
    }

    u32 getId() const
    {
        return mId;
    }

    u32 getIdWithType() const
    {
        SEAD_ASSERT(ItemType::Invalid < mItemType && mItemType < ItemType::InternalFileTypes);

        switch (mItemType)
        {
            case ItemType::Sound:
                return nw::snd::internal::Util::GetMaskedItemId(mId, nw::snd::internal::ItemType_Sound);

            case ItemType::SoundSet:
                return nw::snd::internal::Util::GetMaskedItemId(mId, nw::snd::internal::ItemType_SoundGroup);

            case ItemType::Bank:
                return nw::snd::internal::Util::GetMaskedItemId(mId, nw::snd::internal::ItemType_Bank);

            case ItemType::Player:
                return nw::snd::internal::Util::GetMaskedItemId(mId, nw::snd::internal::ItemType_Player);

            case ItemType::WaveArchive:
                return nw::snd::internal::Util::GetMaskedItemId(mId, nw::snd::internal::ItemType_WaveArchive);

            case ItemType::Group:
                return nw::snd::internal::Util::GetMaskedItemId(mId, nw::snd::internal::ItemType_Group);
        }

        SEAD_ASSERT(false);
        return mId;
    }

    void setId(u32 id)
    {
        mId = id;
    }

    bool isEnableName() const
    {
        return mEnableName;
    }

    void setEnableName(bool enable)
    {
        mEnableName = enable;
    }

    const sead::SafeString& getName() const
    {
        return mName;
    }

    sead::FixedSafeString<256>& getName()
    {
        return mName;
    }

    sead::SafeString getNameOrNull() const
    {
        return !mName.isEmpty() ? mName.cstr() : "(null)";
    }

    virtual sead::FixedSafeString<256> getFormattedName() const
    {
        sead::FixedSafeString<256> name;
        name.appendWithFormat("[%u] ", getId());
        name.appendWithFormat("%s", getNameOrNull().cstr());

        return name;
    }

    bool isNameValid() const
    {
        return mEnableName && !mName.isEmpty();
    }

    const ItemReference::List& getReferences() const
    {
        return mReferences;
    }

    virtual void onOpenFileWindow()
    {
    }

    virtual void onCloseFileWindow()
    {
    }

    virtual bool validate(sead::BufferedSafeString& error) const
    {
        error = "Not implemented";
        return false;
    }

private:
    void addReference_(ItemReference* ref)
    {
        mReferences.pushBack(ref);
    }

protected:
    ItemType mItemType;
    u32 mId;
    bool mEnableName;
    sead::FixedSafeString<256> mName;

    ItemReference::List mReferences;

    friend class ItemReference;

    friend class Bfsar;
};

inline void ItemReference::attach(Item* item)
{
    if (isAttached())
        detach();

    if (item)
    {
        item->addReference_(this);

        mItem = item;
    }
}

inline u32 ItemReference::getItemId() const
{
    if (isAttached())
        return mItem->getId();

    return Item::cInvalidId;
}
