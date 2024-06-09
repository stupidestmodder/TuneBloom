#pragma once

#include <bfsar/Item.h>

struct IdEntry : public sead::TListNode<IdEntry*>
{
    IdEntry(Item* owner, Item* item)
        : sead::TListNode<IdEntry*>(this)
        , mIdRef(owner)
    {
        mIdRef.attach(item);
    };

    u32 getId() const
    {
        return mIdRef.getItemId();
    }

    ItemReference& getIdRef()
    {
        return mIdRef;
    }

private:
    ItemReference mIdRef;
};

class IdTable : public sead::TList<IdEntry*>
{
public:
    IdTable(Item* owner)
        : sead::TList<IdEntry*>()
        , mOwner(owner)
    {
        SEAD_ASSERT(mOwner);
    }

    ~IdTable()
    {
        clear();
    }

    void clear()
    {
        for (auto it = robustBegin(); it != robustEnd(); ++it)
        {
            IdEntry* entry = (*it).val();

            erase(entry);

            delete entry;
        }

        sead::TList<IdEntry*>::clear();
    }

    const Item* getOwner() const
    {
        return mOwner;
    }

    Item* getOwner()
    {
        return mOwner;
    }

private:
    Item* mOwner;
};
