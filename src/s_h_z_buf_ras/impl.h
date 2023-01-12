#ifndef KOUEK_S_H_Z_BUF_RAS_IMPL_H
#define KOUEK_S_H_Z_BUF_RAS_IMPL_H

#include "../ras/impl.h"
#include <h_z_buf_ras.h>

#include <list>
#include <unordered_set>

namespace kouek {

class SimpleHZBufferRasterizerImpl : public RasterizerImpl,
                                     public SimpleHZBufferRasterizer {
  protected:
    static constexpr uint8_t Z_BUF_MIPMAP_LVL_NUM = 6;
    std::array<glm::uvec2, Z_BUF_MIPMAP_LVL_NUM> rndrSzMipmap;
    std::array<size_t, Z_BUF_MIPMAP_LVL_NUM> resolutionMipmap;
    std::array<std::vector<float>, Z_BUF_MIPMAP_LVL_NUM> zbufferMipmap;

    struct EdgeNode {
        std::array<uint8_t, 2> v2;
        // size_t tIdx is NOT needed
        glm::uint ymax;
        float x;
        float dx;
        float coeff;
        float dcoeff;

        bool operator<(const EdgeNode &other) const {
            return x < other.x || (x == other.x && dx < other.dx);
        }
    };
    std::vector<std::list<EdgeNode>> sortedET;
    std::list<EdgeNode> activeEL;

  private:
    std::unordered_set<glm::uint> activeFaceIndices;
    std::unordered_set<glm::uint> tmpAFIs;

  public:
    virtual void SetRenderSize(const glm::uvec2 &rndrSz) override;
    virtual void Render() override;

  private:
    void runRasterization();

  protected:
    inline auto getScrnAABB(const V2R &v2r) {
        glm::uvec2 min{std::numeric_limits<glm::uint>::max()};
        glm::uvec2 max{std::numeric_limits<glm::uint>::min()};
        for (uint8_t v = 0; v < v2r.vCnt; ++v) {
            glm::vec2 xy{v2r.vs[v].pos.x, v2r.vs[v].pos.y};
            xy = glm::round(xy);
            if (min.x > (glm::uint)xy.x)
                min.x = (glm::uint)xy.x;
            if (min.y > (glm::uint)xy.y)
                min.y = (glm::uint)xy.y;
            if (max.x < (glm::uint)xy.x)
                max.x = (glm::uint)xy.x;
            if (max.y < (glm::uint)xy.y)
                max.y = (glm::uint)xy.y;
        }
        for (uint8_t xy = 0; xy < 2; ++xy) {
            if (min[xy] >= rndrSz[xy])
                min[xy] = rndrSz[xy] - 1;
            if (max[xy] >= rndrSz[xy])
                max[xy] = rndrSz[xy] - 1;
        }
        return std::make_tuple(min, max);
    }
    inline uint8_t getMipmapLvl(const glm::uvec2 &min, const glm::uvec2 &max) {
        auto v2 = max - min;
        v2.x = std::min(v2.x, v2.y);
        uint8_t lvl;
        for (uint8_t lvlPlusOne = Z_BUF_MIPMAP_LVL_NUM; lvlPlusOne > 1;
             --lvlPlusOne) {
            lvl = lvlPlusOne - 1;
            if (rndrSzMipmap[lvl].x <= v2.x && rndrSzMipmap[lvl].y <= v2.x)
                break;
        }
        return lvl;
    }
    inline void initSortedET(const V2R &v2r, uint8_t lvl) {
        uint8_t currV = 0, nextV = 1, nnV = 2;
        while (true) {
            if (nnV == v2r.vCnt)
                nnV = 0;
            if (nextV == v2r.vCnt)
                nextV = 0;

            std::array xy2{glm::uvec2{(glm::uint)roundf(v2r.vs[currV].pos.x),
                                      (glm::uint)roundf(v2r.vs[currV].pos.y)},
                           glm::uvec2{(glm::uint)roundf(v2r.vs[nextV].pos.x),
                                      (glm::uint)roundf(v2r.vs[nextV].pos.y)}};
            xy2[0].x = xy2[0].x >> lvl;
            xy2[0].y = xy2[0].y >> lvl;
            xy2[1].x = xy2[1].x >> lvl;
            xy2[1].y = xy2[1].y >> lvl;

            if (xy2[0].y != xy2[1].y) {
                uint8_t bot = 0, top = 1;
                if (xy2[0].y > xy2[1].y) {
                    bot = 1;
                    top = 0;
                }
                EdgeNode node;
                node.v2[bot] = currV;
                node.v2[top] = nextV;
                node.ymax = xy2[top].y;
                if (node.ymax >= rndrSzMipmap[lvl].y)
                    node.ymax = rndrSzMipmap[lvl].y - 1;
                node.coeff = 0.f;
                node.dcoeff = xy2[top].y - xy2[bot].y;
                node.x = xy2[bot].x;
                node.dx = (xy2[top].x > xy2[bot].x
                               ? (float)(xy2[top].x - xy2[bot].x)
                               : -(float)(xy2[bot].x - xy2[top].x)) /
                          node.dcoeff;
                node.dcoeff = 1.f / node.dcoeff;

                auto ymin = xy2[bot].y;
                // Corner Case:
                //    /|
                // P./_|
                //   \ |
                //    \|
                // Here, the horizontal segment won't be drawn
                // if the point P is counted twice
                auto ynn = (glm::uint)roundf(v2r.vs[nnV].pos.y);
                ynn = ynn >> lvl;
                if (ynn < ymin)
                    ++ymin;
                if (ymin >= rndrSzMipmap[lvl].y)
                    ymin = rndrSzMipmap[lvl].y - 1;
                auto itr = std::lower_bound(sortedET[ymin].begin(),
                                            sortedET[ymin].end(), node);
                sortedET[ymin].insert(itr, node);
            }

            if (nextV == 0)
                break;
            currV = nextV;
            ++nextV;
            ++nnV;
        }
    }
    inline void updateZBufferMipmap(glm::uint x, glm::uint y) {
        uint8_t lvl = 0;
        while (lvl < Z_BUF_MIPMAP_LVL_NUM - 1) {
            auto far = std::numeric_limits<float>::min();
            auto idx = (size_t)y * rndrSzMipmap[lvl].x + x;
            if (far < zbufferMipmap[lvl][idx])
                far = zbufferMipmap[lvl][idx];

            if ((x & 0x1) != 0)
                --idx;
            else if (x < rndrSzMipmap[lvl].x - 1)
                ++idx;
            if (far < zbufferMipmap[lvl][idx])
                far = zbufferMipmap[lvl][idx];

            if ((y & 0x1) != 0)
                idx -= rndrSzMipmap[lvl].x;
            else if (y < rndrSzMipmap[lvl].y - 1)
                idx += rndrSzMipmap[lvl].x;
            if (far < zbufferMipmap[lvl][idx])
                far = zbufferMipmap[lvl][idx];

            if ((x & 0x1) != 0)
                ++idx;
            else if (x < rndrSzMipmap[lvl].x - 1)
                --idx;
            if (far < zbufferMipmap[lvl][idx])
                far = zbufferMipmap[lvl][idx];

            x = x >> 1;
            y = y >> 1;
            ++lvl;
            idx = (size_t)y * rndrSzMipmap[lvl].x + x;
            zbufferMipmap[lvl][idx] = far;
        }
    }
    inline void rasFace(const V2R &v2r, const glm::uvec2 &min,
                        const glm::uvec2 &max);
};

} // namespace kouek

#endif // !KOUEK_S_H_Z_BUF_RAS_IMPL_H