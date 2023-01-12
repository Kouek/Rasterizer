#ifndef KOUEK_OCTREE_H
#define KOUEK_OCTREE_H

#include <iostream>
#include <string>

#include <array>
#include <queue>
#include <stack>
#include <vector>

#include <glm/glm.hpp>

namespace kouek {

template <typename IdxTy, uint32_t Cap, uint32_t Height> class Octree {
  public:
    struct AABB {
        glm::vec3 min, max;
    };
    struct LeafDat {
        AABB aabb;
        IdxTy idx;

        LeafDat(const AABB &aabb, IdxTy idx) : aabb{aabb}, idx{idx} {}
    };
    struct Node {
        bool isLeaf;
        AABB aabb;
        AABB looseAABB;
        union {
            std::array<Node *, 8> children;
            std::vector<LeafDat> leafDats;
        };
#ifdef OTREE_NODE_WITH_NAME
        std::string name;
#endif // !OTREE_NODE_WITH_NAME

        Node(bool isLeaf) : isLeaf{isLeaf} {
            if (isLeaf)
                new (&leafDats) std::vector<LeafDat>;
        }
        ~Node() {
            if (isLeaf)
                leafDats.~vector();
        }
    };

  private:
    Node *root;

  public:
    ~Octree() {
        std::stack<Node *> stk;
        stk.emplace(root);
        while (!stk.empty()) {
            auto node = stk.top();
            stk.pop();
            if (!node->isLeaf)
                for (auto child : node->children)
                    stk.emplace(child);
            delete node;
        }
    }
    Octree() : root(new Node(true)) {
#ifdef OTREE_NODE_WITH_NAME
        root->name = "0";
#endif // !OTREE_NODE_WITH_NAME
    }

    auto GetRoot() { return root; }
    IdxTy GetLeafDatNum(Node *node) {
        IdxTy num = 0;
        std::stack<Node *> stk;
        stk.emplace(node);
        while (!stk.empty()) {
            auto top = stk.top();
            stk.pop();
            if (top->isLeaf)
                num += top->leafDats.size();
            else
                for (auto child : top->children)
                    stk.emplace(child);
        }
        return num;
    }
    void Add(const std::vector<AABB> &aabbs,
             const std::vector<IdxTy> &indices) {
        for (IdxTy i = 0; i < indices.size(); ++i) {
            auto [node, par, chIdx, h] = searchWhileUnion(root, aabbs[i]);

            while (true) {
                if (node->leafDats.size() < Cap || h >= Height) {
                    node->leafDats.emplace_back(aabbs[i], indices[i]);
                    break;
                }

                // split
                auto mid = .5f * (node->aabb.min + node->aabb.max);
                decltype(Node::children) children;
                for (uint8_t chIdx = 0; chIdx < 8; ++chIdx) {
                    children[chIdx] = new Node(true);
                    children[chIdx]->aabb = children[chIdx]->looseAABB =
                        genOctCmp(node->aabb, mid, chIdx);
#ifdef OTREE_NODE_WITH_NAME
                    children[chIdx]->name = node->name + std::to_string(chIdx);
#endif // !OTREE_NODE_WITH_NAME
                }
                for (const auto &leafDat : node->leafDats) {
                    auto chIdx = chooseChild(children, leafDat.aabb);
                    unionAABB(children[chIdx]->looseAABB, leafDat.aabb);
                    children[chIdx]->leafDats.emplace_back(leafDat);
                }
                {
                    auto aabb = node->aabb;
                    auto looseAABB = node->looseAABB;
#ifdef OTREE_NODE_WITH_NAME
                    auto name = node->name;
#endif // !OTREE_NODE_WITH_NAME
                    delete node;
                    node = new Node(false);
                    node->aabb = aabb;
                    node->looseAABB = looseAABB;
                    node->children = children;
#ifdef OTREE_NODE_WITH_NAME
                    node->name = name;
#endif // !OTREE_NODE_WITH_NAME
                    if (par == nullptr)
                        root = node;
                    else
                        par->children[chIdx] = node;
                }
                chIdx = chooseChild(children, aabbs[i]);
                unionAABB(children[chIdx]->aabb, aabbs[i]);
                par = node;
                node = children[chIdx];
                ++h;
            }
        }
    }
    void Reset(const AABB &rootAABB) {
        std::stack<Node *> stk;
        stk.emplace(root);
        while (!stk.empty()) {
            auto node = stk.top();
            stk.pop();
            if (!node->isLeaf)
                for (auto child : node->children)
                    stk.emplace(child);
            delete node;
        }

        root = new Node(true);
#ifdef OTREE_NODE_WITH_NAME
        root->name = "0";
#endif // !OTREE_NODE_WITH_NAME
        root->aabb = rootAABB;
        root->looseAABB = rootAABB;
    }

  private:
    static inline AABB genOctCmp(const AABB &parAABB, const glm::vec3 &mid,
                                 uint8_t chIdx) {
        AABB aabb;
        uint8_t and = 0x1;
        for (uint8_t xyz = 0; xyz < 3; ++xyz, and = and << 1) {
            aabb.min[xyz] = (chIdx & and) == 0 ? parAABB.min[xyz] : mid[xyz];
            aabb.max[xyz] = (chIdx & and) != 0 ? parAABB.max[xyz] : mid[xyz];
        }
        return aabb;
    }
    static inline void unionAABB(AABB &s, const AABB &o) {
        for (uint8_t xyz = 0; xyz < 3; ++xyz) {
            if (s.min[xyz] > o.min[xyz])
                s.min[xyz] = o.min[xyz];
            if (s.max[xyz] < o.max[xyz])
                s.max[xyz] = o.max[xyz];
        }
    }
    static inline float isIntersected(const AABB &a, const AABB &b) {
        return !(a.max.x <= b.min.x || a.max.y <= b.min.y ||
                 a.max.z <= b.min.z || a.min.x >= b.max.x ||
                 a.min.y >= b.max.y || a.min.z >= b.max.z);
    }
    static inline float intersectedVol(const AABB &a, const AABB &b) {
        if (!isIntersected(a, b))
            return 0.f;
        auto min = glm::max(a.min, b.min);
        auto max = glm::min(a.max, b.max);
        return (max.x - min.x) * (max.y - min.y) * (max.z - min.z);
    }
    static inline uint8_t chooseChild(const decltype(Node::children) &children,
                                      const AABB &aabb) {
        float maxVol = std::numeric_limits<float>::min();
        uint8_t maxChIdx = 0;
        for (uint8_t chIdx = 0; chIdx < 8; ++chIdx)
            if (auto vol = intersectedVol(children[chIdx]->aabb, aabb);
                maxVol < vol) {
                maxVol = vol;
                maxChIdx = chIdx;
            }
        return maxChIdx;
    }
    inline auto searchWhileUnion(Node *root, const AABB &aabb) {
        Node *node = root;
        Node *par = nullptr;
        uint8_t chIdx = 8;
        decltype(Height) h = 0;
        while (true) {
            if (node != root)
                unionAABB(node->looseAABB, aabb);
            if (node->isLeaf)
                return std::make_tuple(node, par, chIdx, h);
            chIdx = chooseChild(node->children, aabb);
            par = node;
            node = node->children[chIdx];
            ++h;
        }
    }
};

template <typename IdxTy, uint32_t Cap, uint32_t Height>
std::ostream &operator<<(std::ostream &os, Octree<IdxTy, Cap, Height> &otree) {
    decltype(Height) maxH = 0;
    IdxTy maxLeafDatNum = 0;
    IdxTy leafDatTotNum = 0;
    size_t leafNodeNum = 0;
    std::stack<std::tuple<Octree<IdxTy, Cap, Height>::Node *, decltype(Height)>>
        stk;
    stk.emplace(otree.GetRoot(), 0);
    while (!stk.empty()) {
        auto [top, h] = stk.top();
        stk.pop();
        if (top->isLeaf) {
            if (maxLeafDatNum < top->leafDats.size())
                maxLeafDatNum = top->leafDats.size();
            if (maxH < h)
                maxH = h;
            leafDatTotNum += top->leafDats.size();
            ++leafNodeNum;
        } else
            for (auto child : top->children)
                stk.emplace(child, h + 1);
    }
    os << "Octree" << std::endl;
    os << ">> Max Leaf Data Num: " << maxLeafDatNum << std::endl;
    os << ">> Max Height: " << maxH << std::endl;
    os << ">> Leaf Data Total Num: " << leafDatTotNum << std::endl;
    os << ">> Leaf Node Num: " << leafNodeNum << std::endl;
    return os;
}

} // namespace kouek

#endif // !KOUEK_OCTREE_H
