#pragma once

#include <bfsar/Item.h>
#include <bfsar/InnerFile.h>

#include <SeqTextInfo.h>

#include <vector>
#include <string>
#include <unordered_map>

class MmlCommandBase;
class TextEditor;

class SequenceFile : public Item, public InnerFile
{
public:
    struct LabelStartInfo
    {
        u32 offset;

        u32 parsedOffset; // If offset points to a alloctrack command this is offset by +3 bytes
        u32 allocTrackFlags;
    };

    static const u32 cInvaldOffset = 0xFFFFFFFF;

public:
    SequenceFile()
        : Item()
        , InnerFile()
        , mIsValid(false)
        , mIsDirty(false)
        , mTextEditor(nullptr)
        , mSeqTextInfo()
        , mSeqText(nullptr)

        , mSeqBytes(nullptr)
        , mSeqBytesSize(0)
        , mLabelsStartInfo()
        , mLabels()
        , mOffsetToLine()

        , mStartLabel()
    {
        mItemType = ItemType::SequenceFile;

        for (u32 i = 0; i < 4; i++)
        {
            mBankRefs[i] = new ItemReference(this);
        }
    }

    ~SequenceFile() override;

    void onOpenFileWindow() override;
    void onCloseFileWindow() override;

    void drawUI() override;
    void drawFileUI();

private:
    void doRead(const void* fileAddr) override;
    u32 doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const override;

    void compile_(bool setCursorPos = true);
    MmlCommandBase* parseCommand_(const std::string& str, const std::vector<std::string>& args, std::string& errorMsg);

public:
    bool isValid() const
    {
        return mIsValid;
    }

    bool isDirty() const
    {
        return mIsDirty;
    }

    const u8* getSeqBytes() const
    {
        return mSeqBytes;
    }

    u32 getLabelOffset(const sead::SafeString& label) const;
    u32 getLabelAllocTracks(const sead::SafeString& label) const;

    std::string getLabelFromParsedOffset(u32 offset, u32 allocTrackFlags) const;

    const std::vector<std::string>& getLabels() const
    {
        return mLabels;
    }

private:
    bool mIsValid;
    bool mIsDirty;
    TextEditor* mTextEditor;
    SeqTextInfo mSeqTextInfo;
    sead::HeapSafeString* mSeqText;

    u8* mSeqBytes;
    u32 mSeqBytesSize;
    std::unordered_map<std::string, LabelStartInfo> mLabelsStartInfo;
    std::vector<std::string> mLabels;
    std::unordered_map<u32, u32> mOffsetToLine;

    ItemReference* mBankRefs[4];
    sead::FixedSafeString<128> mStartLabel;
};
