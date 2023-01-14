#include "impl.h"

#include <algorithm>
#include <unordered_map>

std::unique_ptr<kouek::Rasterizer>
kouek::HierarchicalZBufferRasterizer::Create() {
    return std::make_unique<HierarchicalZBufferRasterizerImpl>();
}

void kouek::HierarchicalZBufferRasterizerImpl::SetRenderSize(
    const glm::uvec2 &rndrSz) {
    RasterizerImpl::SetRenderSize(rndrSz);

    zbufferMipmap[0].clear();
    zbufferMipmap[0].shrink_to_fit();
    rndrSzMipmap[0] = rndrSz;
    resolutionMipmap[0] = resolution;
    zbufferMipmap[0].reserve(resolution);
    for (uint8_t lvl = 1; lvl < Z_BUF_MIPMAP_LVL_NUM; ++lvl) {
        zbufferMipmap[lvl].clear();
        zbufferMipmap[lvl].shrink_to_fit();
        rndrSzMipmap[lvl] = rndrSzMipmap[lvl - 1] / (glm::uint)2;
        resolutionMipmap[lvl] =
            (size_t)rndrSzMipmap[lvl].x * (size_t)rndrSzMipmap[lvl].y;
        zbufferMipmap[lvl].reserve(resolutionMipmap[lvl]);
    }

    sortedET.clear();
    sortedET.resize(rndrSz.y);
}

void kouek::HierarchicalZBufferRasterizerImpl::SetVertexData(
    std::shared_ptr<std::vector<glm::vec3>> positions,
    std::shared_ptr<std::vector<glm::vec3>> colors,
    std::shared_ptr<std::vector<glm::uint>> indices) {
    RasterizerImpl::SetVertexData(positions, colors, indices);

    std::vector<decltype(otree)::AABB> aabbs;
    aabbs.reserve(triangleNum);
    std::vector<glm::uint> is;
    is.reserve(triangleNum);

    decltype(otree)::AABB rootAABB;
    for (uint8_t xyz = 0; xyz < 3; ++xyz) {
        rootAABB.min[xyz] = std::numeric_limits<float>::max();
        rootAABB.max[xyz] = std::numeric_limits<float>::min();
    }
    for (glm::uint fIdx = 0; fIdx < triangleNum; ++fIdx) {
        auto idxIdx = fIdx * 3;
        std::array<glm::uint, 3> vIdx3;
        vIdx3[0] = (*indices)[idxIdx + 0];
        vIdx3[1] = (*indices)[idxIdx + 1];
        vIdx3[2] = (*indices)[idxIdx + 2];

        aabbs.emplace_back();
        auto &aabb = aabbs.back();
        for (uint8_t xyz = 0; xyz < 3; ++xyz) {
            aabb.min[xyz] = std::numeric_limits<float>::max();
            aabb.max[xyz] = std::numeric_limits<float>::min();
        }
        for (uint8_t v = 0; v < 3; ++v)
            for (uint8_t xyz = 0; xyz < 3; ++xyz) {
                if (aabb.min[xyz] > (*positions)[vIdx3[v]][xyz])
                    aabb.min[xyz] = (*positions)[vIdx3[v]][xyz];
                if (aabb.max[xyz] < (*positions)[vIdx3[v]][xyz])
                    aabb.max[xyz] = (*positions)[vIdx3[v]][xyz];
                if (rootAABB.min[xyz] > (*positions)[vIdx3[v]][xyz])
                    rootAABB.min[xyz] = (*positions)[vIdx3[v]][xyz];
                if (rootAABB.max[xyz] < (*positions)[vIdx3[v]][xyz])
                    rootAABB.max[xyz] = (*positions)[vIdx3[v]][xyz];
            }

        is.emplace_back(fIdx);
    }

    otree.Reset(rootAABB);
    otree.Add(aabbs, is);
    std::cout << otree << std::endl;
}

void kouek::HierarchicalZBufferRasterizerImpl::Render() {
    runPreRasterization();
    runRasterization();
}

void kouek::HierarchicalZBufferRasterizerImpl::runRasterization() {
    colorOutput.assign(resolution, glm::zero<glm::u8vec4>());
    for (uint8_t lvl = 0; lvl < Z_BUF_MIPMAP_LVL_NUM; ++lvl)
        zbufferMipmap[lvl].assign(resolutionMipmap[lvl],
                                  std::numeric_limits<float>::infinity());
#ifdef SHOW_DRAW_FACE_NUM
    glm::uint drawFaceCnt = 0;
#endif // SHOW_DRAW_FACE_NUM
    tmpALNs = std::move(activeLeafNodes);
    for (auto node : tmpALNs)
        if (depTestOctreeNode(node->looseAABB)) {
            for (auto &leafDat : node->leafDats)
                if (v2rs[leafDat.idx].vCnt != 0) {
                    auto [min, max] = getScrnAABB(v2rs[leafDat.idx]);
#ifdef SHOW_DRAW_FACE_NUM
                    ++drawFaceCnt;
#endif // SHOW_DRAW_FACE_NUM
                    rasFace(v2rs[leafDat.idx], min, max);
                }
            activeLeafNodes.emplace(node);
        }

    stk.emplace(otree.GetRoot());
    while (!stk.empty()) {
        auto node = stk.top();
        stk.pop();

        if (tmpALNs.find(node) != tmpALNs.end())
            continue;

        if (node->isLeaf && node->leafDats.empty())
            continue;

        // Depth Test
        if (!depTestOctreeNode(node->looseAABB))
            continue;

        if (node->isLeaf) {
            for (auto &leafDat : node->leafDats)
                if (v2rs[leafDat.idx].vCnt != 0) {
                    auto [min, max] = getScrnAABB(v2rs[leafDat.idx]);
#ifdef SHOW_DRAW_FACE_NUM
                    ++drawFaceCnt;
#endif // SHOW_DRAW_FACE_NUM
                    rasFace(v2rs[leafDat.idx], min, max);
                }
            activeLeafNodes.emplace(node);
        } else
            for (auto child : node->children)
                stk.emplace(child);
    }

#ifdef SHOW_DRAW_FACE_NUM
    std::cout << ">> draw face num (ras):\t" << drawFaceCnt << std::endl;
#endif // SHOW_DRAW_FACE_NUM
}

bool kouek::HierarchicalZBufferRasterizerImpl::depTestOctreeNode(
    const decltype(otree)::AABB &aabb) {
    bool pass = false;

    static constexpr std::array indices{// -Z
                                        1, 0, 2, 2, 3, 1,
                                        // +Z
                                        4, 5, 7, 7, 6, 4,
                                        // -Y
                                        0, 1, 5, 5, 4, 0,
                                        // +Y
                                        6, 7, 3, 3, 2, 6,
                                        // -X
                                        0, 4, 6, 6, 2, 0,
                                        // +X
                                        5, 1, 3, 3, 7, 5};
    std::array<glm::vec4, 8> positions;
    for (uint8_t v = 0; v < 8; ++v) {
        positions[v].x = (v & 0x1) == 0 ? aabb.min.x : aabb.max.x;
        positions[v].y = (v & 0x2) == 0 ? aabb.min.y : aabb.max.y;
        positions[v].z = (v & 0x4) == 0 ? aabb.min.z : aabb.max.z;
        positions[v].w = 1.f;
        positions[v] = MVP * positions[v];

        positions[v].w = 1.f / positions[v].w;
        positions[v].x *= positions[v].w;
        positions[v].y *= positions[v].w;
        positions[v].z *= positions[v].w;
    }

    std::array<glm::uvec2, 3> triangle;
    for (uint8_t f = 0; f < 6; ++f) {
        auto i = f * 6;
        // Face Culling
        {
            glm::vec3 v0v1{
                positions[indices[i + 1]].x - positions[indices[i]].x,
                positions[indices[i + 1]].y - positions[indices[i]].y,
                positions[indices[i + 1]].z - positions[indices[i]].z};
            glm::vec3 v0v2{
                positions[indices[i + 2]].x - positions[indices[i]].x,
                positions[indices[i + 2]].y - positions[indices[i]].y,
                positions[indices[i + 2]].z - positions[indices[i]].z};
            v0v1 = glm::cross(v0v1, v0v2);
            if (v0v1.z < 0) // cull back face
                continue;
        }

        for (uint8_t t = 0; t < 2; ++t, i += 3) {
            glm::uvec2 min{std::numeric_limits<glm::uint>::max()};
            glm::uvec2 max{std::numeric_limits<glm::uint>::min()};

            auto computeMinMax = [&](uint8_t v, uint8_t lvl) {
                if (min.x > triangle[v].x)
                    min.x = triangle[v].x;
                if (min.y > triangle[v].y)
                    min.y = triangle[v].y;
                if (max.x < triangle[v].x)
                    max.x = triangle[v].x;
                if (max.y < triangle[v].y)
                    max.y = triangle[v].y;

                if (max.x >= rndrSzMipmap[lvl].x && rndrSzMipmap[lvl].x != 0)
                    max.x = rndrSzMipmap[lvl].x - 1;
                if (max.y >= rndrSzMipmap[lvl].y && rndrSzMipmap[lvl].y != 0)
                    max.y = rndrSzMipmap[lvl].y - 1;
            };

            for (uint8_t v = 0; v < 3; ++v) {
                triangle[v].x = (glm::uint)roundf(
                    (positions[indices[i + v]].x + 1.f) * .5f * rndrSz.x);
                triangle[v].y = (glm::uint)roundf(
                    (positions[indices[i + v]].y + 1.f) * .5f * rndrSz.y);
                computeMinMax(v, 0);
            }

            auto lvl = getMipmapLvl(min, max);
            if (lvl == 0)
                return true;
            min.x = std::numeric_limits<glm::uint>::max();
            min.y = std::numeric_limits<glm::uint>::max();
            max.x = std::numeric_limits<glm::uint>::min();
            max.y = std::numeric_limits<glm::uint>::min();

            for (uint8_t v = 0; v < 3; ++v) {
                triangle[v].x = triangle[v].x >> lvl;
                triangle[v].y = triangle[v].y >> lvl;
                computeMinMax(v, lvl);
            }

            std::array<uint8_t, 3> v3;
            v3[0] = 0;
            while (v3[0] < 3) {
                v3[1] = v3[0] + 1;
                if (v3[1] == 3)
                    v3[1] = 0;
                v3[2] = v3[1] + 1;
                if (v3[2] == 3)
                    v3[2] = 0;

                if (triangle[v3[0]].y != triangle[v3[1]].y) {
                    uint8_t bot = 0, top = 1;
                    if (triangle[v3[0]].y > triangle[v3[1]].y) {
                        bot = 1;
                        top = 0;
                    }
                    EdgeNode node;
                    node.v2[bot] = v3[0];
                    node.v2[top] = v3[1];
                    node.ymax = triangle[top].y;
                    if (node.ymax >= rndrSzMipmap[lvl].y)
                        node.ymax = rndrSzMipmap[lvl].y - 1;
                    node.coeff = 0.f;
                    node.dcoeff = triangle[v3[top]].y - triangle[v3[bot]].y;
                    node.x = triangle[v3[bot]].x;
                    node.dx = (triangle[v3[top]].x > triangle[v3[bot]].x
                                   ? (float)(triangle[v3[top]].x -
                                             triangle[v3[bot]].x)
                                   : -(float)(triangle[v3[bot]].x -
                                              triangle[v3[top]].x)) /
                              node.dcoeff;
                    node.dcoeff = 1.f / node.dcoeff;

                    auto ymin = triangle[v3[bot]].y;
                    // Corner Case:
                    //    /|
                    // P./_|
                    //   \ |
                    //    \|
                    // Here, the horizontal segment won't be drawn
                    // if the point P is counted twice
                    auto ynn = (glm::uint)roundf(triangle[v3[2]].y);
                    ynn = ynn >> lvl;
                    if (ynn < ymin)
                        ++ymin;
                    if (ymin >= rndrSzMipmap[lvl].y)
                        ymin = rndrSzMipmap[lvl].y - 1;
                    auto itr = std::lower_bound(sortedET[ymin].begin(),
                                                sortedET[ymin].end(), node);
                    sortedET[ymin].insert(itr, node);
                }

                ++v3[0];
            }

            // Scanline Rendering
            size_t rowLftIdx = (size_t)min.y * (size_t)rndrSzMipmap[lvl].x;
            glm::uint y;
            for (y = min.y; y <= max.y; ++y, rowLftIdx += rndrSzMipmap[lvl].x) {
                for (auto &edge : activeEL) {
                    edge.x += edge.dx;
                    edge.coeff += edge.dcoeff;
                }
                activeEL.splice(activeEL.begin(), sortedET[y]);
                if (activeEL.empty())
                    continue;
                activeEL.sort();

                // there are only 2 elements in activeEL
                // since the primitive is convex
                auto L = activeEL.begin();
                auto R = L;
                std::advance(R, 1);
                if (R == activeEL.end())
                    goto FINAL_PER_Y;

                std::array oneMinusCoeff2{1.f - L->coeff, 1.f - R->coeff};
                std::array dep2{
                    positions[indices[i + L->v2[0]]].z * oneMinusCoeff2[0] +
                        positions[indices[i + L->v2[1]]].z * L->coeff,
                    positions[indices[i + R->v2[0]]].z * oneMinusCoeff2[1] +
                        positions[indices[i + R->v2[1]]].z * R->coeff};
                std::array LRLimit{(glm::uint)roundf(L->x),
                                   (glm::uint)roundf(R->x)};
                if (LRLimit[1] >= max.x && max.x != 0)
                    LRLimit[1] = max.x - 1;

                auto scnLnCoeff = 0.f;
                auto scnLnDCoeff = LRLimit[1] == LRLimit[0]
                                       ? 0
                                       : 1.f / (float)(LRLimit[1] - LRLimit[0]);
                for (glm::uint x = LRLimit[0]; x <= LRLimit[1];
                     ++x, scnLnCoeff += scnLnDCoeff) {
                    auto oneMinusScnLnCoeff = 1.f - scnLnCoeff;
                    auto dep =
                        dep2[0] * oneMinusScnLnCoeff + dep2[1] * scnLnCoeff;
                    if (dep <= zbufferMipmap[lvl][rowLftIdx + x]) {
                        pass = true;
                        goto FINAL_PER_TRIANGLE;
                    }
                }

            FINAL_PER_Y:
                for (R = activeEL.begin(); R != activeEL.end();)
                    if (R->ymax == y)
                        activeEL.erase(R++);
                    else
                        ++R;
            }

        FINAL_PER_TRIANGLE:
            activeEL.clear();
            for (; y <= max.y; ++y)
                sortedET[y].clear();
            if (pass)
                return true;
        }
    }

    return false;
}
