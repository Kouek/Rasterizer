#include "impl.h"

#include <algorithm>
#include <unordered_map>

std::unique_ptr<kouek::Rasterizer> kouek::ZBufferRasterizer::Create() {
    return std::make_unique<ZBufferRasterizerImpl>();
}

void kouek::ZBufferRasterizerImpl::SetRenderSize(const glm::uvec2 &rndrSz) {
    RasterizerImpl::SetRenderSize(rndrSz);

    zbuffer.clear();
    zbuffer.shrink_to_fit();
    zbuffer.reserve(resolution);

    sortedET.clear();
    sortedET.resize(rndrSz.y);
}

void kouek::ZBufferRasterizerImpl::Render() {
    runPreRasterization();
    runRasterization();
}

void kouek::ZBufferRasterizerImpl::runRasterization() {
    colorOutput.assign(resolution, glm::zero<glm::u8vec4>());
    zbuffer.assign(resolution, std::numeric_limits<float>::infinity());

    // Init Sorted Edge Tables
    for (glm::uint tIdx = 0; tIdx < v2rs.size(); ++tIdx) {
        auto &v2r = v2rs[tIdx];
        if (v2r.vCnt == 0)
            continue;

        uint8_t currV = 0, nextV = 1, nnV = 2;
        while (true) {
            if (nnV == v2r.vCnt)
                nnV = 0;
            if (nextV == v2r.vCnt)
                nextV = 0;

            std::array xy2{glm::vec2{v2r.vs[currV].pos.x, v2r.vs[currV].pos.y},
                           glm::vec2{v2r.vs[nextV].pos.x, v2r.vs[nextV].pos.y}};
            xy2[0] = glm::round(xy2[0]);
            xy2[1] = glm::round(xy2[1]);

            if (xy2[0].y != xy2[1].y) {
                uint8_t bot = 0, top = 1;
                if (xy2[0].y > xy2[1].y) {
                    bot = 1;
                    top = 0;
                }
                EdgeNode node;
                node.tIdx = tIdx;
                node.v2[bot] = currV;
                node.v2[top] = nextV;
                node.ymax = (glm::uint)xy2[top].y;
                if (node.ymax >= rndrSz.y)
                    node.ymax = rndrSz.y - 1;
                node.coeff = 0.f;
                node.dcoeff = xy2[top].y - xy2[bot].y;
                node.x = xy2[bot].x;
                node.dx = (xy2[top].x - xy2[bot].x) / node.dcoeff;
                node.dcoeff = 1.f / node.dcoeff;

                auto ymin = (glm::uint)xy2[bot].y;
                // Corner Case:
                //    /|
                // P./_|
                //   \ |
                //    \|
                // Here, the horizontal segment won't be drawn
                // if the point P is counted twice
                if (v2r.vs[nnV].pos.y < ymin)
                    ++ymin;
                if (ymin >= rndrSz.y)
                    ymin = rndrSz.y - 1;
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

    // Scanline Rendering
    std::unordered_map<glm::uint, decltype(activeEL)::iterator> pairMaps;
    size_t rowLftIdx = 0;
    for (glm::uint y = 0; y < rndrSz.y; ++y, rowLftIdx += rndrSz.x) {
        for (auto &edge : activeEL) {
            edge.x += edge.dx;
            edge.coeff += edge.dcoeff;
        }
        activeEL.splice(activeEL.begin(), sortedET[y]);
        if (activeEL.empty())
            continue;
        activeEL.sort();

        auto R = activeEL.begin();
        pairMaps.emplace(std::piecewise_construct,
                         std::forward_as_tuple(R->tIdx),
                         std::forward_as_tuple(R));
        ++R;
        while (R != activeEL.end()) {
            auto pairedItr = pairMaps.find(R->tIdx);
            if (pairedItr == pairMaps.end()) {
                pairMaps.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(R->tIdx),
                                 std::forward_as_tuple(R));
                ++R;
                continue;
            }

            auto L = pairedItr->second;
            auto &v2r = v2rs[R->tIdx];
            std::array oneMinusCoeff2{1.f - L->coeff, 1.f - R->coeff};
            std::array dep2{v2r.vs[L->v2[0]].pos.z * oneMinusCoeff2[0] +
                                v2r.vs[L->v2[1]].pos.z * L->coeff,
                            v2r.vs[R->v2[0]].pos.z * oneMinusCoeff2[1] +
                                v2r.vs[R->v2[1]].pos.z * R->coeff};
            std::array rhw2{v2r.vs[L->v2[0]].pos.w * oneMinusCoeff2[0] +
                                v2r.vs[L->v2[1]].pos.w * L->coeff,
                            v2r.vs[R->v2[0]].pos.w * oneMinusCoeff2[1] +
                                v2r.vs[R->v2[1]].pos.w * R->coeff};
            std::array<V2RDat::SurfDat, 2> surf2;
            if (uvs) {
                surf2[0].uv = v2r.vs[L->v2[0]].surf.uv * oneMinusCoeff2[0] +
                              v2r.vs[L->v2[1]].surf.uv * L->coeff;
                surf2[1].uv = v2r.vs[R->v2[0]].surf.uv * oneMinusCoeff2[1] +
                              v2r.vs[R->v2[1]].surf.uv * R->coeff;
            } else if (colors) {
                surf2[0].col = v2r.vs[L->v2[0]].surf.col * oneMinusCoeff2[0] +
                               v2r.vs[L->v2[1]].surf.col * L->coeff;
                surf2[1].col = v2r.vs[R->v2[0]].surf.col * oneMinusCoeff2[1] +
                               v2r.vs[R->v2[1]].surf.col * R->coeff;
            }
            std::array<glm::vec4, 2> norm2, wdPos2;
            if (norms) {
                norm2[0] = v2r.vs[L->v2[0]].norm * oneMinusCoeff2[0] +
                           v2r.vs[L->v2[1]].norm * L->coeff;
                norm2[1] = v2r.vs[R->v2[0]].norm * oneMinusCoeff2[1] +
                           v2r.vs[R->v2[1]].norm * R->coeff;
                wdPos2[0] = v2r.vs[L->v2[0]].wdPos * oneMinusCoeff2[0] +
                            v2r.vs[L->v2[1]].wdPos * L->coeff;
                wdPos2[1] = v2r.vs[R->v2[0]].wdPos * oneMinusCoeff2[1] +
                            v2r.vs[R->v2[1]].wdPos * R->coeff;
            }
            std::array LRLimit{(glm::uint)roundf(L->x),
                               (glm::uint)roundf(R->x)};
            if (LRLimit[1] >= rndrSz.x)
                LRLimit[1] = rndrSz.x - 1;

            pairMaps.erase(pairedItr);
            ++R;

            auto scnLnCoeff = 0.f;
            auto scnLnDCoeff = LRLimit[1] == LRLimit[0]
                                   ? 0
                                   : 1.f / (float)(LRLimit[1] - LRLimit[0]);
            for (glm::uint x = LRLimit[0]; x <= LRLimit[1];
                 ++x, scnLnCoeff += scnLnDCoeff) {
                auto oneMinusScnLnCoeff = 1.f - scnLnCoeff;
                auto dep = dep2[0] * oneMinusScnLnCoeff + dep2[1] * scnLnCoeff;

                // Depth Test
                if (dep >= zbuffer[rowLftIdx + x])
                    continue; // reject
                zbuffer[rowLftIdx + x] = dep;

                // Perspective Correction (Cont.)
                auto rhw = rhw2[0] * oneMinusScnLnCoeff + rhw2[1] * scnLnCoeff;
                rhw = 1.f / rhw;

                V2RDat::SurfDat surf;
                if (uvs) {
                    surf.uv = surf2[0].uv * oneMinusScnLnCoeff +
                              surf2[1].uv * scnLnCoeff;
                    surf.uv *= rhw;
                } else if (colors) {
                    surf.col = surf2[0].col * oneMinusScnLnCoeff +
                               surf2[1].col * scnLnCoeff;
                    surf.col *= rhw;
                }

                glm::vec4 norm, wdPos;
                if (norms) {
                    norm =
                        norm2[0] * oneMinusScnLnCoeff + norm2[1] * scnLnCoeff;
                    wdPos =
                        wdPos2[0] * oneMinusScnLnCoeff + wdPos2[1] * scnLnCoeff;
                    norm *= rhw;
                    wdPos *= rhw;
                }

                // Coloring
                glm::vec3 color{1.f, 1.f, 1.f};
                if (uvs)
                    ;
                else if (colors)
                    color = surf.col;

                // Shading
                if (norms) {
                    auto ambient = light.ambientStrength * light.ambientColor;
                    glm::vec3 N{norm};
                    N = glm::normalize(N);
                    glm::vec3 lightDir{light.position.x - wdPos.x,
                                       light.position.y - wdPos.y,
                                       light.position.z - wdPos.z};
                    lightDir = glm::normalize(lightDir);
                    auto diff = std::max(glm::dot(N, lightDir), 0.f);
                    auto diffuse = diff * light.color;
                    colorOutput[rowLftIdx + x] =
                        rgbF2rgbaU8((ambient + diffuse) * color);
                } else
                    colorOutput[rowLftIdx + x] = rgbF2rgbaU8(color);
            }
        }
        pairMaps.clear();

        for (R = activeEL.begin(); R != activeEL.end();)
            if (R->ymax == y)
                activeEL.erase(R++);
            else
                ++R;
    }
}
