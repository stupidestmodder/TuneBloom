#include <framework/seadMethodTreeMgr.h>
#include <heap/seadExpHeap.h>
#include <heap/seadFrameHeap.h>
#include <heap/seadHeap.h>
#include <heap/seadHeapMgr.h>
#include <heap/seadUnboundHeap.h>
#include <prim/seadScopedLock.h>
#include <thread/seadThread.h>

#include "imgui/imgui.h"

static void DoHeapInfo(const sead::Heap* heap)
{
    if (ImGui::TreeNode(heap->getName().cstr()))
    {
        if (sead::DynamicCast<const sead::ExpHeap>(heap))
            ImGui::Text("Type: ExpHeap");
        else if (sead::DynamicCast<const sead::FrameHeap>(heap))
            ImGui::Text("Type: FrameHeap");
        else if (sead::DynamicCast<const sead::UnboundHeap>(heap))
            ImGui::Text("Type: UnboundHeap");
        else
            ImGui::Text("Type: Heap");

        size_t size = heap->getSize();
        ImGui::Text("Total Size: %.2fMB (0x%X B)", size / 1024.0f / 1024.0f, size);

        if (!sead::DynamicCast<const sead::UnboundHeap>(heap))
        {
            size_t freeSize = heap->getFreeSize();
            size_t allocatableSize = heap->getMaxAllocatableSize();
            size_t usedSize = size - freeSize;

            ImGui::Text("Used Size: %.2fMB (0x%X B)", usedSize / 1024.0f / 1024.0f, usedSize);
            ImGui::Text("Free Size: %.2fMB (0x%X B)", freeSize / 1024.0f / 1024.0f, freeSize);
            ImGui::Text("Max Allocatable: %.2fMB (0x%X B)", allocatableSize / 1024.0f / 1024.0f, allocatableSize);
        }
        else
        {
            ImGui::Text("Active Allocations: %d", sead::DynamicCast<const sead::UnboundHeap>(heap)->getMemBlockListSize());
        }

        ImGui::Text("Start Address: 0x%p", heap->getStartAddress());
        ImGui::Text("End Address: 0x%p", heap->getEndAddress());
        ImGui::Text("Is Empty: %s", heap->isEmpty() ? "true" : "false");

        if (ImGui::TreeNode("Children"))
        {
            for (auto it = heap->childBegin(); it != heap->childEnd(); ++it)
            {
                DoHeapInfo(&(*it));
            }

            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
}

void HeapInfo()
{
    if (ImGui::CollapsingHeader("HeapMgr"))
    {
        sead::HeapMgr* heapMgr = sead::HeapMgr::instance();

        for (s32 i = 0; i < heapMgr->getRootHeapNum(); i++)
        {
            DoHeapInfo(heapMgr->getRootHeap(i));
        }

        DoHeapInfo(heapMgr->getUnboundHeap());
    }
}

static void MethodTreeInfo(const sead::MethodTreeMgr* tree)
{
    if (ImGui::CollapsingHeader("MethodTreeMgr"))
    {
        SEAD_UNUSED(tree);
    }
}

static void DoThreadInfo(const sead::Thread* thread)
{
    if (ImGui::TreeNode(thread->getName().cstr()))
    {
        ImGui::Text("Priority: %d", thread->getPriority());
        ImGui::Text("Thread ID: %u", thread->getID());
        ImGui::Text("Stack Size: %d", thread->getStackSize());
        ImGui::Text("Stack Used Size Peak: %d", thread->calcStackUsedSizePeak());
        ImGui::Text("Block Type: %s", thread->getBlockType() == sead::MessageQueue::BlockType::eBlock ? "eBlock" : "eNoBlock");
        ImGui::Text("isDone: %s", thread->isDone() ? "true" : "false");

        ImGui::TreePop();
    }
}

static void ThreadInfo()
{
    if (ImGui::CollapsingHeader("ThreadMgr"))
    {
        sead::ThreadMgr* threadMgr = sead::ThreadMgr::instance();

        DoThreadInfo(threadMgr->getMainThread());

        sead::ScopedLock<sead::CriticalSection> lock(threadMgr->getIterateLockCS());
        for (auto it = threadMgr->constBegin(); it != threadMgr->constEnd(); ++it)
        {
            DoThreadInfo(*it);
        }
    }
}
