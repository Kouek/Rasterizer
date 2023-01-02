#ifndef KOUEK_Z_BUF_RAS_IMPL_H
#define KOUEK_Z_BUF_RAS_IMPL_H

#include "../ras/impl.h"
#include <z_buf_ras.h>

#include <list>

namespace kouek {

class ZBufferRasterizerImpl : public RasterizerImpl, public ZBufferRasterizer {
  private:
    std::vector<float> zbuffer;

    struct EdgeNode {
        std::array<uint8_t, 2> v2;
        size_t tIdx;
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

  public:
    virtual void SetRenderSize(const glm::uvec2 &rndrSz) override;
    virtual void Render() override;

  private:
    void runRasterization();
};

} // namespace kouek

#endif // !KOUEK_Z_BUF_RAS_IMPL_H