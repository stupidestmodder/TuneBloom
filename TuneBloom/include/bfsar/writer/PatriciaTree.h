#pragma once

#include <vector>
#include <string>

#include <basis/seadTypes.h>

struct PatriciaTree
{
    struct NodeData
    {
        NodeData(u32 string, u32 item)
            : stringId(string)
            , itemId(item)
        {
        }

        static const u32 INVALID_STRING_ID = 0xFFFFFFFF;
        static const u32 INVALID_ITEM_ID = 0xFFFFFFFF;

        u32 stringId;
        u32 itemId;
    };

    struct Node
    {
        Node(const std::string k, u16 flag, u16 bits, u32 parent, u32 left, u32 right, NodeData data, u32 idx)
            : key(k)
            , flags(flag)
            , bit(bits)
            , parentIdx(parent)
            , leftIdx(left)
            , rightIdx(right)
            , nodeData(data)
            , myIdx(idx)
        {
        }

        static const u16 FLAG_LEAF = 1 << 0;
        static const u32 INVALID_IDX = 0xFFFFFFFF;

        u32 getParentIdx() const
        {
            return parentIdx;
        }

        u32 getLeftIdx() const
        {
            return leftIdx;
        }

        void setLeftIdx(std::vector<Node>& nodeTable, u32 idx)
        {
            if (idx == leftIdx)
                return;

            if (leftIdx != INVALID_IDX)
            {
                Node& left = nodeTable[leftIdx];
                left.parentIdx = INVALID_IDX;
            }

            if (idx != INVALID_IDX)
            {
                Node& node = nodeTable[idx];
                if (node.parentIdx != INVALID_IDX)
                {
                    SEAD_ASSERT(false);
                }

                node.parentIdx = myIdx;
            }

            leftIdx = idx;
        }

        u32 getRightIdx() const
        {
            return rightIdx;
        }

        void setRightIdx(std::vector<Node>& nodeTable, u32 idx)
        {
            if (idx == rightIdx)
                return;

            if (rightIdx != INVALID_IDX)
            {
                Node& right = nodeTable[rightIdx];
                right.parentIdx = INVALID_IDX;
            }

            if (idx != INVALID_IDX)
            {
                Node& node = nodeTable[idx];
                if (node.parentIdx != INVALID_IDX)
                {
                    SEAD_ASSERT(false);
                }

                node.parentIdx = myIdx;
            }

            rightIdx = idx;
        }

        std::string key;
        u16 flags;
        u16 bit;
        NodeData nodeData;

    private:
        u32 parentIdx;
        u32 leftIdx;
        u32 rightIdx;
        u32 myIdx;
    };

    PatriciaTree()
        : rootIdx(Node::INVALID_IDX)
    {
    }

    u32 rootIdx;
    std::vector<Node> nodeTable;

    void insert(const std::string& key, const NodeData& data)
    {
        insert(rootIdx, key, data);
    }

    void insert(u32 nodeIdx, const std::string& key, const NodeData& data)
    {
        if (nodeIdx == Node::INVALID_IDX)
        {
            Node node(key, Node::FLAG_LEAF, GetBitCount(key), Node::INVALID_IDX, Node::INVALID_IDX, Node::INVALID_IDX, data, nodeTable.size());

            nodeTable.push_back(node);

            rootIdx = nodeTable.size() - 1;

            return;
        }

        const Node& node = nodeTable[nodeIdx];

        u16 start = 0;
        if (node.getParentIdx() != Node::INVALID_IDX)
        {
            const Node& parent = nodeTable[node.getParentIdx()];
            start = parent.bit;
        }

        if (node.flags & Node::FLAG_LEAF)
        {
            u16 bit = CompareBit(node.key, key, start);
            if (bit == 0)
            {
                SEAD_ASSERT(false);
            }

            Node leaf(key, Node::FLAG_LEAF, GetBitCount(key), Node::INVALID_IDX, Node::INVALID_IDX, Node::INVALID_IDX, data, nodeTable.size());
            nodeTable.push_back(leaf);

            branch(nodeIdx, nodeTable.size() - 1, bit);
        }
        else
        {
            u16 bit = CompareBit(node.key, key, start, node.bit);
            if (bit != 0xFFFF)
            {
                Node leaf(key, Node::FLAG_LEAF, GetBitCount(key), Node::INVALID_IDX, Node::INVALID_IDX, Node::INVALID_IDX, data, nodeTable.size());
                nodeTable.push_back(leaf);

                branch(nodeIdx, nodeTable.size() - 1, bit);
            }
            else
            {
                insert(GetBit(key, node.bit) ? node.getRightIdx() : node.getLeftIdx(), key, data);
            }
        }
    }

    void branch(u32 nodeIdx, u32 newNodeIdx, u16 bit)
    {
        const Node* node = &nodeTable[nodeIdx];
        const Node* newNode = &nodeTable[newNodeIdx];

        if (GetBit(node->key, bit) == GetBit(newNode->key, bit))
        {
            SEAD_ASSERT(false);
        }

        NodeData data(NodeData::INVALID_STRING_ID, NodeData::INVALID_ITEM_ID);
        Node node1(GetBranchKey(*node, *newNode, bit), 0, bit, Node::INVALID_IDX, Node::INVALID_IDX, Node::INVALID_IDX, data, nodeTable.size());

        nodeTable.push_back(node1);
        u32 node1Idx = nodeTable.size() - 1;

        // Reassign cuz the push_back can reallocate and move the items
        node = &nodeTable[nodeIdx];
        newNode = &nodeTable[newNodeIdx];

        if (node->getParentIdx() != Node::INVALID_IDX)
        {
            Node& parent = nodeTable[node->getParentIdx()];
            if (parent.getLeftIdx() == nodeIdx)
                parent.setLeftIdx(nodeTable, node1Idx);
            else
                parent.setRightIdx(nodeTable, node1Idx);
        }
        else
        {
            rootIdx = node1Idx;
        }

        Node& node1Real = nodeTable[node1Idx];
        if (node->bit > bit && GetBit(node->key, bit))
        {
            node1Real.setLeftIdx(nodeTable, newNodeIdx);
            node1Real.setRightIdx(nodeTable, nodeIdx);
        }
        else
        {
            node1Real.setLeftIdx(nodeTable, nodeIdx);
            node1Real.setRightIdx(nodeTable, newNodeIdx);
        }
    }

private:
    std::string GetBranchKey(const Node& node1, const Node& node2, u16 bit)
    {
        return SubString(GetBitCount(node1.key) <= bit ? node2.key : node1.key, bit);
    }

    std::string SubString(const std::string& str, u16 bit)
    {
        u16 length = bit / 8;

        if (0 < bit - length * 8)
            length++;

        return str.substr(0, length);
    }

    u16 GetBitCount(const std::string& str)
    {
        return str.size() * 8;
    }

    u16 CompareBit(const std::string& str1, const std::string& str2, u16 start)
    {
        return CompareBit(str1, str2, start, -1);
    }

    u16 CompareBit(const std::string& str1, const std::string& str2, u16 start, u16 end)
    {
        if (start < 0)
        {
            SEAD_ASSERT(false);
        }

        u16 num = end;

        if (end < 0)
            num = GetBitCount(str1.size() < str2.size() ? str2 : str1);

        for (u16 bit = start; bit < num; bit++)
        {
            if (GetBit(str1, bit) ^ GetBit(str2, bit))
                return bit;
        }

        return 0xFFFF;
    }

    bool GetBit(char ch, u16 bit)
    {
        if (bit < 0 || bit > 7)
        {
            SEAD_ASSERT(false);
        }

        return 0 != ((u32)ch >> 7 - bit & 1);
    }

    bool GetBit(const std::string& str, u16 bit)
    {
        if (bit < 0)
        {
            SEAD_ASSERT(false);
        }

        u16 index = bit / 8;
        return GetBit(index < str.size() ? str[index] : 0, bit - index * 8);
    }
};
