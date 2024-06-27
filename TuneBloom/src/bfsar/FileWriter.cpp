#include <bfsar/writer/FileWriter.h>

#include <snd/snd_Util.h>

#include <math/seadMathCalcCommon.h>

void FileWriter::openFile(const sead::SafeString& magic, u32 numBlocks, u32 version)
{
    mBlocks.allocBuffer(numBlocks);

    mHeaderPos = getPosition();

    mStream->writeString(magic, magic.calcLength());

    mStream->writeU16(0xFEFF); // Endian

    mHeaderSize = sizeof(nw::ut::BinaryFileHeader);
    mHeaderSize += numBlocks * sizeof(nw::snd::internal::Util::ReferenceWithSize);
    mHeaderSize = sead::MathCalcCommon<u16>::roundUpPow2(mHeaderSize, 0x20);

    mStream->writeU16(mHeaderSize);

    mStream->writeU32(version);

    mStream->writeU32(0); // File Size

    mStream->writeU16(numBlocks);

    mStream->writeU16(0); // Reserved

    for (u32 i = 0; i < numBlocks; i++)
    {
        nw::snd::internal::Util::ReferenceWithSize ref;
        sead::MemUtil::fillZero(&ref, sizeof(ref));

        mStream->writeMemBlock(&ref, sizeof(ref));
    }

    u32 padBytes = mHeaderSize - getPosition();
    for (u32 i = 0; i < padBytes; i++)
    {
        mStream->writeU8(0);
    }
}

void FileWriter::closeFile()
{
    if (mBlockOpen)
    {
        SEAD_ASSERT_MSG(false, "tried to close file with open block");
        return;
    }

    if (mReferences.size() > 0)
    {
        SEAD_ASSERT_MSG(false, "tried to close file with open References");
        return;
    }

    if (mSizedReferences.size() > 0)
    {
        SEAD_ASSERT_MSG(false, "tried to close file with open SizedReferences");
        return;
    }

    if (mReferenceTables.size() > 0)
    {
        SEAD_ASSERT_MSG(false, "tried to close file with open ReferenceTables");
        return;
    }

    if (mSizedReferenceTables.size() > 0)
    {
        SEAD_ASSERT_MSG(false, "tried to close file with open SizedReferenceTables");
        return;
    }

    if (mOffsetBases.size() > 0)
    {
        SEAD_ASSERT_MSG(false, "tried to close file with OffsetBases mismatch");
        return;
    }

    u32 size = mHeaderSize;
    for (const Block* block = mBlocks.front(); block; block = mBlocks.next(block))
    {
        size += block->size;
    }

    seek(mHeaderPos + offsetof(nw::ut::BinaryFileHeader, fileSize));
    mStream->writeU32(size);

    //? Write blocks
    seek(mHeaderPos + sizeof(nw::ut::BinaryFileHeader));

    s32 offset = mHeaderSize;
    for (const Block* block = mBlocks.front(); block; block = mBlocks.next(block))
    {
        mStream->writeU16(block->type);
        mStream->writeU16(0); // Padding

        if (block->size != 0)
        {
            mStream->writeS32(offset);
            mStream->writeU32(block->size);
        }
        else
        {
            mStream->writeS32(nw::snd::internal::Util::Reference::INVALID_OFFSET);
            mStream->writeU32(0xFFFFFFFF);
        }

        offset += block->size;
    }

    //? Restore position to the end
    seek(mHeaderPos + size);
}

void FileWriter::openBlock(u16 typeId, const sead::SafeString& magic)
{
    if (mBlockOpen)
    {
        SEAD_ASSERT_MSG(false, "block open/close mismatch");
        return;
    }

    Block* block = mBlocks.birthBack();
    SEAD_ASSERT(block);

    block->type = typeId;
    block->size = 0;

    mBlockPos = getPosition();

    mOffsetBase = mBlockPos + sizeof(nw::ut::BinaryBlockHeader);

    mStream->writeString(magic, magic.calcLength());

    mStream->writeU32(0); // Size

    mBlockOpen = true;
}

void FileWriter::closeBlock()
{
    if (!mBlockOpen)
    {
        SEAD_ASSERT_MSG(false, "block open/close mismatch");
        return;
    }

    if (mOffsetBases.size() > 0)
    {
        SEAD_ASSERT_MSG(false, "tried to close block with OffsetBases mismatch");
        return;
    }

    u32 blockSize = getPosition() - mBlockPos;

    seek(mBlockPos + offsetof(nw::ut::BinaryBlockHeader, size));
    mStream->writeU32(blockSize);

    Block* block = mBlocks.back();
    SEAD_ASSERT(block);

    block->size = blockSize;

    seek(mBlockPos + blockSize);

    mBlockOpen = false;
}

void FileWriter::nullBlock(u16 typeId)
{
    Block* block = mBlocks.birthBack();
    block->type = typeId;
    block->size = 0;
}

void FileWriter::align(u32 alignment)
{
    if (!mBlockOpen)
    {
        SEAD_ASSERT_MSG(false, "block not open");
        return;
    }

    while ((getPosition() - mBlockPos) % alignment != 0)
    {
        mStream->writeU8(0);
    }
}

void FileWriter::openReference(const sead::SafeString& name)
{
    if (mReferences.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "reference already exists");
        return;
    }

    u32 pos = getPosition();

    {
        nw::snd::internal::Util::Reference ref;
        sead::MemUtil::fillZero(&ref, sizeof(ref));

        mStream->writeMemBlock(&ref, sizeof(ref));
    }

    mReferences[name.cstr()] = Reference(pos, mOffsetBase - mBlockPos);
}

void FileWriter::closeReference(const sead::SafeString& name, u16 typeId)
{
    if (!mReferences.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "reference not found");
        return;
    }

    const Reference& ref = mReferences[name.cstr()];

    s32 offset = getPosition() - ref.offset - mBlockPos;

    u32 prevPos = getPosition();
    seek(ref.pos);

    mStream->writeU16(typeId);
    mStream->writeU16(0); // Padding
    mStream->writeS32(offset);

    seek(prevPos);

    mReferences.erase(name.cstr());
}

void FileWriter::closeReference(const sead::SafeString& name, u16 typeId, s32 offset)
{
    if (!mReferences.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "reference not found");
        return;
    }

    const Reference& ref = mReferences[name.cstr()];

    u32 prevPos = getPosition();
    seek(ref.pos);

    mStream->writeU16(typeId);
    mStream->writeU16(0); // Padding
    mStream->writeS32(offset);

    seek(prevPos);

    mReferences.erase(name.cstr());
}

void FileWriter::closeNullReference(const sead::SafeString& name)
{
    if (!mReferences.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "reference not found");
        return;
    }

    const Reference& ref = mReferences[name.cstr()];

    s32 offset = nw::snd::internal::Util::Reference::INVALID_OFFSET;

    u32 prevPos = getPosition();
    seek(ref.pos);

    mStream->writeU16(0); // Type
    mStream->writeU16(0); // Padding
    mStream->writeS32(offset);

    seek(prevPos);

    mReferences.erase(name.cstr());
}

void FileWriter::openSizedReference(const sead::SafeString& name)
{
    if (mSizedReferences.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "sized reference already exists");
        return;
    }

    u32 pos = getPosition();

    {
        nw::snd::internal::Util::ReferenceWithSize ref;
        sead::MemUtil::fillZero(&ref, sizeof(ref));

        mStream->writeMemBlock(&ref, sizeof(ref));
    }

    mSizedReferences[name.cstr()] = Reference(pos, mOffsetBase - mBlockPos);
}

void FileWriter::closeSizedReference(const sead::SafeString& name, u16 typeId, u32 size)
{
    if (!mSizedReferences.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "sized reference not found");
        return;
    }

    const Reference& ref = mSizedReferences[name.cstr()];

    s32 offset = getPosition() - ref.offset - mBlockPos - size;

    u32 prevPos = getPosition();
    seek(ref.pos);

    mStream->writeU16(typeId);
    mStream->writeU16(0); // Padding
    mStream->writeS32(offset);
    mStream->writeU32(size);

    seek(prevPos);

    mSizedReferences.erase(name.cstr());
}

void FileWriter::closeSizedReference(const sead::SafeString& name, u16 typeId, s32 offset, u32 size)
{
    if (!mSizedReferences.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "sized reference not found");
        return;
    }

    const Reference& ref = mSizedReferences[name.cstr()];

    u32 prevPos = getPosition();
    seek(ref.pos);

    mStream->writeU16(typeId);
    mStream->writeU16(0); // Padding
    mStream->writeS32(offset);
    mStream->writeU32(size);

    seek(prevPos);

    mSizedReferences.erase(name.cstr());
}

void FileWriter::closeNullSizedReference(const sead::SafeString& name)
{
    if (!mSizedReferences.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "sized reference not found");
        return;
    }

    const Reference& ref = mSizedReferences[name.cstr()];

    s32 offset = nw::snd::internal::Util::Reference::INVALID_OFFSET;

    u32 prevPos = getPosition();
    seek(ref.pos);

    mStream->writeU16(0); // Type
    mStream->writeU16(0); // Padding
    mStream->writeS32(offset);
    mStream->writeU32(0xFFFFFFFF); // Size

    seek(prevPos);

    mSizedReferences.erase(name.cstr());
}

void FileWriter::pushOffsetBase()
{
    mOffsetBases.push(mOffsetBase);

    mOffsetBase = getPosition();
}

void FileWriter::popOffsetBase()
{
    mOffsetBase = mOffsetBases.top();
    mOffsetBases.pop();
}

void FileWriter::openReferenceTable(const sead::SafeString& name, u32 count)
{
    if (mReferenceTables.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "reference table already exists");
        return;
    }

    u32 pos = getPosition();

    {
        mStream->writeU32(count);

        for (u32 i = 0; i < count; i++)
        {
            nw::snd::internal::Util::Reference ref;
            sead::MemUtil::fillZero(&ref, sizeof(ref));

            mStream->writeMemBlock(&ref, sizeof(ref));
        }
    }

    mReferenceTables[name.cstr()] = ReferenceTable(count, pos, mOffsetBase - mBlockPos);
}

void FileWriter::closeReferenceTable(const sead::SafeString& name)
{
    if (!mReferenceTables.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "reference table not found");
        return;
    }

    const ReferenceTable& table = mReferenceTables[name.cstr()];

    u32 prevPos = getPosition();
    seek(table.pos + offsetof(nw::snd::internal::Util::ReferenceTable, item));

    for (u32 i = 0; i < table.references.size(); i++)
    {
        const ReferenceTable::Reference& ref = table.references[i];

        mStream->writeU16(ref.type);
        mStream->writeU16(0); // Padding
        mStream->writeS32(ref.offset);
    }

    seek(prevPos);

    mReferenceTables.erase(name.cstr());
}

void FileWriter::addReferenceTableReference(const sead::SafeString& name, u16 typeId)
{
    if (!mReferenceTables.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "reference table not found");
        return;
    }

    ReferenceTable& table = mReferenceTables[name.cstr()];

    table.add(typeId, getPosition() - table.offset - mBlockPos);
}

void FileWriter::addReferenceTableNullReference(const sead::SafeString& name, u16 typeId)
{
    if (!mReferenceTables.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "reference table not found");
        return;
    }

    ReferenceTable& table = mReferenceTables[name.cstr()];

    table.add(typeId, -1);
}

void FileWriter::openSizedReferenceTable(const sead::SafeString& name, u32 count)
{
    if (mSizedReferenceTables.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "sized reference table already exists");
        return;
    }

    u32 pos = getPosition();

    {
        mStream->writeU32(count);

        for (u32 i = 0; i < count; i++)
        {
            nw::snd::internal::Util::ReferenceWithSize ref;
            sead::MemUtil::fillZero(&ref, sizeof(ref));

            mStream->writeMemBlock(&ref, sizeof(ref));
        }
    }

    mSizedReferenceTables[name.cstr()] = ReferenceTable(count, pos, mOffsetBase - mBlockPos);
}

void FileWriter::closeSizedReferenceTable(const sead::SafeString& name)
{
    if (!mSizedReferenceTables.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "sized reference table not found");
        return;
    }

    const ReferenceTable& table = mSizedReferenceTables[name.cstr()];

    u32 prevPos = getPosition();
    seek(table.pos + offsetof(nw::snd::internal::Util::ReferenceWithSizeTable, item));

    for (u32 i = 0; i < table.references.size(); i++)
    {
        const ReferenceTable::Reference& ref = table.references[i];

        mStream->writeU16(ref.type);
        mStream->writeU16(0); // Padding
        mStream->writeS32(ref.offset);
        mStream->writeU32(ref.size);
    }

    seek(prevPos);

    mSizedReferenceTables.erase(name.cstr());
}

void FileWriter::addSizedReferenceTableReference(const sead::SafeString& name, u16 typeId, u32 size)
{
    if (!mSizedReferenceTables.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "sized reference table not found");
        return;
    }

    ReferenceTable& table = mSizedReferenceTables[name.cstr()];

    table.add(typeId, getPosition() - table.offset - mBlockPos, size);
}

void FileWriter::setSizedReferenceTableReferenceSize(const sead::SafeString& name, u32 size)
{
    if (!mSizedReferenceTables.contains(name.cstr()))
    {
        SEAD_ASSERT_MSG(false, "sized reference table not found");
        return;
    }

    ReferenceTable& table = mSizedReferenceTables[name.cstr()];

    table.adjustSize(size);
}
