#include "impl.h"

void kouek::RasterizerImpl::SetVertexData(
    std::shared_ptr<std::vector<glm::vec3>> positions,
    std::shared_ptr<std::vector<glm::vec3>> colors,
    std::shared_ptr<std::vector<glm::uint>> indices) {
    this->positions = positions;
    this->colors = colors;
    this->indices = indices;

    triangleNum = indices->size() / 3;

    v2rs.resize(triangleNum);
}

void kouek::RasterizerImpl::SetTextureData(
    std::shared_ptr<std::vector<glm::vec2>> uvs,
    std::shared_ptr<std::vector<glm::uint>> uvIndices,
    std::shared_ptr<std::vector<glm::vec3>> norms,
    std::shared_ptr<std::vector<glm::uint>> nIndices) {
    this->uvs = uvs;
    this->uvIndices = uvIndices;
    this->norms = norms;
    this->nIndices = nIndices;
}

void kouek::RasterizerImpl::SetRenderSize(const glm::uvec2 &rndrSz) {
    this->rndrSz = rndrSz;
    resolution = (size_t)rndrSz.x * (size_t)rndrSz.y;

    colorOutput.clear();
    colorOutput.shrink_to_fit();
    colorOutput.reserve(resolution);
}

const std::vector<glm::u8vec4> &kouek::RasterizerImpl::GetColorOutput() {
    return colorOutput;
}

void kouek::RasterizerImpl::runPreRasterization() {
    auto pos32pos4 = [](glm::vec4 &o, const glm::vec3 &in) {
        o.x = in.x;
        o.y = in.y;
        o.z = in.z;
        o.w = 1.f;
    };
    auto dir32dir4 = [](glm::vec4 &o, const glm::vec3 &in) {
        o.x = in.x;
        o.y = in.y;
        o.z = in.z;
        o.w = 0.f;
    };
    auto runVertexShader = [&](glm::uint tIdx) {
        std::array<glm::uint, 3> vIdx3;
        std::array<glm::uint, 3> uvIdx3;
        std::array<glm::uint, 3> nIdx3;
        {
            auto idxIdx = tIdx * 3;
            vIdx3[0] = (*indices)[idxIdx + 0];
            vIdx3[1] = (*indices)[idxIdx + 1];
            vIdx3[2] = (*indices)[idxIdx + 2];
            if (uvs) {
                uvIdx3[0] = (*uvIndices)[idxIdx + 0];
                uvIdx3[1] = (*uvIndices)[idxIdx + 1];
                uvIdx3[2] = (*uvIndices)[idxIdx + 2];
            }
            if (norms) {
                nIdx3[0] = (*nIndices)[idxIdx + 0];
                nIdx3[1] = (*nIndices)[idxIdx + 1];
                nIdx3[2] = (*nIndices)[idxIdx + 2];
            }
        }

        for (uint8_t t = 0; t < 3; ++t) {
            auto &v2rDat = v2rs[tIdx].vs[t];
            pos32pos4(v2rDat.pos, (*positions)[vIdx3[t]]);

            if (uvs)
                v2rDat.surf.uv = (*uvs)[uvIdx3[t]];
            else if (colors)
                v2rDat.surf.col = (*colors)[vIdx3[t]];

            if (norms) {
                dir32dir4(v2rDat.norm, (*norms)[nIdx3[t]]);
                pos32pos4(v2rDat.wdPos, (*positions)[vIdx3[t]]);
            }

            // Local Space -> Camera Space
            v2rDat.pos = MVP * v2rDat.pos;
            if (norms) {
                v2rDat.norm = M * v2rDat.norm;
                v2rDat.wdPos = M * v2rDat.wdPos;
            }

            // Near Plane Clip (Easy ver.)
            if (v2rDat.pos.w <= 0.f)
                return false;

            // Perspective Correction on Vertex Attribute
            v2rDat.pos.w = 1.f / v2rDat.pos.w;
            v2rDat.pos.x *= v2rDat.pos.w;
            v2rDat.pos.y *= v2rDat.pos.w;
            v2rDat.pos.z *= v2rDat.pos.w;

            if (uvs)
                v2rDat.surf.uv *= v2rDat.pos.w;
            else if (colors)
                v2rDat.surf.col *= v2rDat.pos.w;

            if (norms) {
                v2rDat.norm *= v2rDat.pos.w;
                v2rDat.wdPos *= v2rDat.pos.w;
            }
        }

        return true;
    };
    auto runSutherlandHodgemanClip = [&](V2R &v2r) {
        std::array<V2RDat, 9> tmp;
        v2r.vCnt = 3;

        // k = 0,1,2,3,4,5 for L,R,D,T,B,F faces
        for (uint8_t k = 0; k < 6; ++k) {
            for (uint8_t k = 0; k < v2r.vCnt; ++k)
                tmp[k] = v2r.vs[k];
            unsigned char currValidVertCnt = 0;
            unsigned char xyz = k >> 1;
            unsigned char SIn, PIn;
            unsigned char S = v2r.vCnt - 1, P = 0;
            float p, q;
            for (; P != v2r.vCnt; S = P, ++P) {
                if ((k & 0x1) == 0) {
                    SIn = tmp[S].pos[xyz] < -1.f ? 0 : 1;
                    PIn = tmp[P].pos[xyz] < -1.f ? 0 : 1;
                    p = tmp[S].pos[xyz] - tmp[P].pos[xyz];
                    q = tmp[S].pos[xyz] - (-1.f);
                } else {
                    SIn = tmp[S].pos[xyz] > 1.f ? 0 : 1;
                    PIn = tmp[P].pos[xyz] > 1.f ? 0 : 1;
                    p = tmp[P].pos[xyz] - tmp[S].pos[xyz];
                    q = 1.f - tmp[S].pos[xyz];
                }

                if (SIn && PIn) {
                    // S and P are in-bound, add P
                    v2r.vs[currValidVertCnt] = tmp[P];
                    ++currValidVertCnt;
                } else if (!SIn && !PIn)
                    ; // S and P are out-bound, don't add
                else {
                    // one of S and P is out-bound, add the clipped vertex,
                    // which is computed with linear interpolation
                    float t = q / p;
                    v2r.vs[currValidVertCnt].pos =
                        tmp[S].pos + t * (tmp[P].pos - tmp[S].pos);

                    if (uvs)
                        v2r.vs[currValidVertCnt].surf.uv =
                            tmp[S].surf.uv +
                            t * (tmp[P].surf.uv - tmp[S].surf.uv);
                    else if (colors)
                        v2r.vs[currValidVertCnt].surf.col =
                            tmp[S].surf.col +
                            t * (tmp[P].surf.col - tmp[S].surf.col);

                    if (norms) {
                        v2r.vs[currValidVertCnt].norm =
                            tmp[S].norm + t * (tmp[P].norm - tmp[S].norm);
                        v2r.vs[currValidVertCnt].wdPos =
                            tmp[S].wdPos + t * (tmp[P].wdPos - tmp[S].wdPos);
                    }
                    ++currValidVertCnt;

                    if (!SIn) {
                        // S is out-bound while P is not, add P
                        v2r.vs[currValidVertCnt] = tmp[P];
                        ++currValidVertCnt;
                    }
                }
            }
            v2r.vCnt = currValidVertCnt;
            if (v2r.vCnt == 0)
                return;
        }
    };

#ifdef SHOW_DRAW_FACE_NUM
    glm::uint drawFaceCnt = 0;
#endif // SHOW_DRAW_FACE_NUM
    for (glm::uint fIdx = 0; fIdx < triangleNum; ++fIdx) {
        auto &v2r = v2rs[fIdx];
        v2r.vCnt = 0;

        // Vertex Shader
        bool nearPlaneClipOK = runVertexShader(fIdx);
        if (!nearPlaneClipOK)
            continue;

        // Face Culling
#ifndef NO_BACK_FACE_CULL
        {
            glm::vec3 v0v1{v2r.vs[1].pos.x - v2r.vs[0].pos.x,
                           v2r.vs[1].pos.y - v2r.vs[0].pos.y,
                           v2r.vs[1].pos.z - v2r.vs[0].pos.z};
            glm::vec3 v0v2{v2r.vs[2].pos.x - v2r.vs[0].pos.x,
                           v2r.vs[2].pos.y - v2r.vs[0].pos.y,
                           v2r.vs[2].pos.z - v2r.vs[0].pos.z};
            v0v1 = glm::cross(v0v1, v0v2);
            if (v0v1.z < 0) // cull back face
                continue;
        }
#endif // !NO_BACK_FACE_CULL

        // Perspective Clipping
        runSutherlandHodgemanClip(v2r);
        if (v2r.vCnt == 0)
            continue;

#ifdef SHOW_DRAW_FACE_NUM
        ++drawFaceCnt;
#endif // SHOW_DRAW_FACE_NUM

        // Camera Space -> Screen Space
        for (uint8_t v = 0; v < v2r.vCnt; ++v) {
            v2r.vs[v].pos.x = (v2r.vs[v].pos.x + 1.f) * .5f * rndrSz.x;
            v2r.vs[v].pos.y = (v2r.vs[v].pos.y + 1.f) * .5f * rndrSz.y;
        }
    }

#ifdef SHOW_DRAW_FACE_NUM
    std::cout << ">> draw face num (v):\t" << drawFaceCnt << std::endl;
#endif // SHOW_DRAW_FACE_NUM
}
