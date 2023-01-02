#ifndef KOUEK_MESH_H
#define KOUEK_MESH_H

#include <fstream>
#include <stdexcept>
#include <string>

#include <array>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace kouek {

class Mesh {
  private:
    std::vector<glm::vec3> vs;
    std::vector<glm::vec2> vts;
    std::vector<glm::vec3> vns;
    std::vector<glm::uvec3> fvs;
    std::vector<glm::uvec3> fvts;
    std::vector<glm::uvec3> fvns;

  public:
    const auto &GetVS() const { return vs; }
    const auto &GetVTS() const { return vts; }
    const auto &GetVNS() const { return vns; }
    const auto &GetFVS() const { return fvs; }
    const auto &GetFVTS() const { return fvts; }
    const auto &GetFVNS() const { return fvns; }
    void ReadFromFile(const std::string &path, bool swapXYZ = false) {
        Clear();

        using namespace std;
        ifstream in(path.c_str());

        if (!in.is_open())
            throw std::runtime_error(
                string(path) + " is NOT a valid path. Load model failed.");

        string buf;
        while (getline(in, buf)) {
            if (buf[0] == 'v' && buf[1] == ' ') {
                double x, y, z;
                sscanf(buf.c_str() + 2, "%lf%lf%lf", &x, &y, &z);
                if (swapXYZ)
                    vs.emplace_back(z, x, y);
                else
                    vs.emplace_back(x, y, z);
            } else if (buf[0] == 'v' && buf[1] == 't' && buf[2] == ' ') {
                double x, y;
                sscanf(buf.c_str() + 3, "%lf%lf", &x, &y);
                vts.emplace_back(x, y);
            } else if (buf[0] == 'v' && buf[1] == 'n' && buf[2] == ' ') {
                double x, y, z;
                sscanf(buf.c_str() + 3, "%lf%lf%lf", &x, &y, &z);
                if (swapXYZ)
                    vns.emplace_back(z, x, y);
                else
                    vns.emplace_back(x, y, z);
            } else if (buf[0] == 'f' && buf[1] == ' ') {
                bool hasUV = false, hasN = false;
                std::array<uint32_t, 4> vIds{0, 0, 0, 0};
                std::array<uint32_t, 4> tIds{0, 0, 0, 0};
                std::array<uint32_t, 4> nIds{0, 0, 0, 0};

                sscanf(buf.c_str() + 2, "%d/%d/%d", &vIds[0],
                                  &tIds[0], &nIds[0]);
                if (tIds[0] != 0)
                    hasUV = true;
                if (nIds[0] != 0)
                    hasN = true;
                auto spIdx = buf.find(' ', 2);
                sscanf(buf.c_str() + spIdx + 1, "%d/%d/%d", &vIds[1], &tIds[1],
                       &nIds[1]);
                spIdx = buf.find(' ', spIdx + 1);
                sscanf(buf.c_str() + spIdx + 1, "%d/%d/%d", &vIds[2], &tIds[2],
                       &nIds[2]);
                spIdx = buf.find(' ', spIdx + 1);

                bool isQuad = false;
                if (spIdx < buf.size() && buf[spIdx + 1] != '\0') {
                    sscanf(buf.c_str() + spIdx + 1, "%d/%d/%d", &vIds[3],
                           &tIds[3], &nIds[3]);
                    isQuad = true;
                }

                for (uint8_t i = 0; i < 4; ++i) {
                    --vIds[i];
                    --tIds[i];
                    --nIds[i];
                }

                fvs.emplace_back(vIds[0], vIds[1], vIds[2]);
                if (hasUV)
                    fvts.emplace_back(tIds[0], tIds[1], tIds[2]);
                if (hasN)
                    fvns.emplace_back(nIds[0], nIds[1], nIds[2]);
                if (isQuad) {
                    fvs.emplace_back(vIds[0], vIds[2], vIds[3]);
                    if (hasUV)
                        fvts.emplace_back(tIds[0], tIds[2], tIds[3]);
                    if (hasN)
                        fvns.emplace_back(nIds[0], nIds[2], nIds[3]);
                }
            }
        }
        in.close();

        if (vs.empty() || fvs.empty())
            throw std::runtime_error(
                "File has no vertices or faces. Load model failed.");

        if (fvns.empty())
            generateNorms();
    }

    inline void Clear() {
        vs.clear();
        vs.shrink_to_fit();
        vts.clear();
        vts.shrink_to_fit();
        vns.clear();
        vns.shrink_to_fit();
        fvs.clear();
        fvs.shrink_to_fit();
        fvts.clear();
        fvts.shrink_to_fit();
        fvns.clear();
        fvns.shrink_to_fit();
    }

  private:
    void generateNorms() {
        for (size_t faceIdx = 0; faceIdx < fvs.size(); ++faceIdx) {
            const auto &vIdx3 = fvs[faceIdx];
            const auto &v0 = vs[vIdx3[1]] - vs[vIdx3[0]];
            const auto &v1 = vs[vIdx3[2]] - vs[vIdx3[0]];
            vns.emplace_back(v0.y * v1.z - v1.y * v0.z,
                             v0.z * v1.x - v1.z * v0.x,
                             v0.x * v1.y - v1.x * v0.y);
            fvns.emplace_back(faceIdx, faceIdx, faceIdx);
        }
    }
};

} // namespace kouek

#endif // !KOUEK_MESH_H
