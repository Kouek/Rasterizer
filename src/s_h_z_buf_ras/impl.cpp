#include "impl.h"

#include <algorithm>
#include <unordered_map>

std::unique_ptr<kouek::Rasterizer> kouek::SimpleHZBufferRasterizer::Create() {
    return std::make_unique<SimpleHZBufferRasterizerImpl>();
}

void kouek::SimpleHZBufferRasterizerImpl::SetRenderSize(
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

void kouek::SimpleHZBufferRasterizerImpl::Render() {
    runPreRasterization();
    runRasterization();
}

void kouek::SimpleHZBufferRasterizerImpl::runRasterization() {
    auto depTestFace = [&](const V2R &v2r, glm::uvec2 min, glm::uvec2 max) {
        auto lvl = getMipmapLvl(min, max);
        for (uint8_t xy = 0; xy < 2; ++xy) {
            min[xy] = min[xy] >> lvl;
            max[xy] = max[xy] >> lvl;
        }

        initSortedET(v2r, lvl);

        // Scanline Rendering
        bool pass = false;
        size_t rowLftIdx = (size_t)min.y * (size_t)rndrSzMipmap[lvl].x;
        for (glm::uint y = min.y; y <= max.y;
             ++y, rowLftIdx += rndrSzMipmap[lvl].x) {
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
            std::array dep2{v2r.vs[L->v2[0]].pos.z * oneMinusCoeff2[0] +
                                v2r.vs[L->v2[1]].pos.z * L->coeff,
                            v2r.vs[R->v2[0]].pos.z * oneMinusCoeff2[1] +
                                v2r.vs[R->v2[1]].pos.z * R->coeff};
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
                auto dep = dep2[0] * oneMinusScnLnCoeff + dep2[1] * scnLnCoeff;
                if (dep <= zbufferMipmap[lvl][rowLftIdx + x]) {
                    pass = true;
                    break;
                }
            }

        FINAL_PER_Y:
            for (R = activeEL.begin(); R != activeEL.end();)
                if (R->ymax == y)
                    activeEL.erase(R++);
                else
                    ++R;
        }

        activeEL.clear();
        for (glm::uint y = min.y; y <= max.y; ++y)
            sortedET[y].clear();

        return pass;
    };

    colorOutput.assign(resolution, glm::zero<glm::u8vec4>());
    for (uint8_t lvl = 0; lvl < Z_BUF_MIPMAP_LVL_NUM; ++lvl)
        zbufferMipmap[lvl].assign(resolutionMipmap[lvl],
                                  std::numeric_limits<float>::infinity());

#ifdef SHOW_DRAW_FACE_NUM
    glm::uint skipCnt = 0;
#endif // SHOW_DRAW_FACE_NUM

    auto run = [&](glm::uint fIdx) {
        auto &v2r = v2rs[fIdx];
        if (v2r.vCnt == 0)
            return false;

        auto [min, max] = getScrnAABB(v2r);

        // Depth Test
        if (!depTestFace(v2r, min, max))
            return false;

#ifdef SHOW_DRAW_FACE_NUM
        ++skipCnt;
#endif // SHOW_DRAW_FACE_NUM
        rasFace(v2r, min, max);
        return true;
    };

    tmpAFIs = std::move(activeFaceIndices);
    for (auto fIdx : tmpAFIs)
        if (run(fIdx))
            activeFaceIndices.emplace(fIdx);

    for (glm::uint fIdx = 0; fIdx < v2rs.size(); ++fIdx)
        if (tmpAFIs.find(fIdx) == tmpAFIs.end())
            if (run(fIdx))
                activeFaceIndices.emplace(fIdx);

#ifdef SHOW_DRAW_FACE_NUM
    std::cout << ">> draw face num (ras):\t" << skipCnt << std::endl;
#endif // SHOW_DRAW_FACE_NUM
}

inline void kouek::SimpleHZBufferRasterizerImpl::rasFace(
    const V2R &v2r, const glm::uvec2 &min, const glm::uvec2 &max) {
    initSortedET(v2r, 0);

    // Scanline Rendering
    size_t rowLftIdx = (size_t)min.y * (size_t)rndrSz.x;
    for (glm::uint y = min.y; y <= max.y; ++y, rowLftIdx += rndrSz.x) {
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
        std::array LRLimit{(glm::uint)roundf(L->x), (glm::uint)roundf(R->x)};
        if (LRLimit[1] >= max.x && max.x != 0)
            LRLimit[1] = max.x - 1;

        auto scnLnCoeff = 0.f;
        auto scnLnDCoeff = LRLimit[1] == LRLimit[0]
                               ? 0
                               : 1.f / (float)(LRLimit[1] - LRLimit[0]);
        for (glm::uint x = LRLimit[0]; x <= LRLimit[1];
             ++x, scnLnCoeff += scnLnDCoeff) {
            auto oneMinusScnLnCoeff = 1.f - scnLnCoeff;
            auto dep = dep2[0] * oneMinusScnLnCoeff + dep2[1] * scnLnCoeff;

            // Depth Test (Cont.)
            if (dep >= zbufferMipmap[0][rowLftIdx + x])
                continue; // reject
            zbufferMipmap[0][rowLftIdx + x] = dep;
            updateZBufferMipmap(x, y);

            // Perspective Correction (Cont.)
            auto rhw = rhw2[0] * oneMinusScnLnCoeff + rhw2[1] * scnLnCoeff;
            rhw = 1.f / rhw;

            V2RDat::SurfDat surf;
            if (uvs) {
                surf.uv =
                    surf2[0].uv * oneMinusScnLnCoeff + surf2[1].uv * scnLnCoeff;
                surf.uv *= rhw;
            } else if (colors) {
                surf.col = surf2[0].col * oneMinusScnLnCoeff +
                           surf2[1].col * scnLnCoeff;
                surf.col *= rhw;
            }

            glm::vec4 norm, wdPos;
            if (norms) {
                norm = norm2[0] * oneMinusScnLnCoeff + norm2[1] * scnLnCoeff;
                wdPos = wdPos2[0] * oneMinusScnLnCoeff + wdPos2[1] * scnLnCoeff;
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

    FINAL_PER_Y:
        for (R = activeEL.begin(); R != activeEL.end();)
            if (R->ymax == y)
                activeEL.erase(R++);
            else
                ++R;
    }
}
