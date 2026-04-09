#pragma once

#include <container/seadRingBuffer.h>
#include <heap/seadDisposer.h>
#include <prim/seadSafeString.h>

class Item;

class PopupMgr
{
    SEAD_SINGLETON_DISPOSER(PopupMgr);

public:
    struct PopupInfo
    {
    private:
        struct Text : public sead::FixedSafeString<1024>
        {
            Text()
            {
            }

            Text(const char* str)
                : sead::FixedSafeString<1024>(str)
            {
            }

            Text(const sead::SafeString& str)
                : sead::FixedSafeString<1024>(str)
            {
            }
        };

    public:
        Text text;
        Item* item = nullptr;
        Item* super = nullptr;
    };

private:
    PopupMgr();

public:
    void addPopup(const PopupInfo& info);
    void update();

    void setCorruptInfo(const sead::SafeString& info)
    {
        mCorruptInfo = info;
    }

    const sead::SafeString& getCorruptInfo() const
    {
        return mCorruptInfo;
    }

private:
    sead::FixedRingBuffer<PopupInfo, 10> mPopups;
    bool mPopupOpen;

    sead::FixedSafeString<1024> mCorruptInfo;
};
