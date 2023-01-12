#ifndef KOUEK_H_Z_BUF_RAS_IMPL_H
#define KOUEK_H_Z_BUF_RAS_IMPL_H

#include "../s_h_z_buf_ras/impl.h"
#include <h_z_buf_ras.h>

#include <util/octree.hpp>

namespace kouek {

class HierarchicalZBufferRasterizerImpl : public SimpleHZBufferRasterizerImpl,
                                          public HierarchicalZBufferRasterizer {
  private:
    Octree<glm::uint, 512, 10> otree;
    std::stack<decltype(otree)::Node *> stk;
    std::unordered_set<decltype(otree)::Node *> activeLeafNodes;
    std::unordered_set<decltype(otree)::Node *> tmpALNs;

  public:
    virtual void SetRenderSize(const glm::uvec2 &rndrSz) override;
    virtual void
    SetVertexData(std::shared_ptr<std::vector<glm::vec3>> positions,
                  std::shared_ptr<std::vector<glm::vec3>> colors,
                  std::shared_ptr<std::vector<glm::uint>> indices) override;
    virtual void Render() override;

  private:
    void runRasterization();
    bool depTestOctreeNode(const decltype(otree)::AABB &aabb);
};

} // namespace kouek

#endif // !KOUEK_H_Z_BUF_RAS_IMPL_H