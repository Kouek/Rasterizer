#ifndef KOUEK_RAS_IMPL_H
#define KOUEK_RAS_IMPL_H

#include <rasterizer.h>

#include <array>

namespace kouek {

class RasterizerImpl : virtual public Rasterizer {
  protected:
    size_t triangleNum;

    glm::uvec2 rndrSz;
    size_t resolution;

    glm::mat4 M = glm::identity<glm::mat4>();
    glm::mat4 V = glm::identity<glm::mat4>();
    glm::mat4 P = glm::identity<glm::mat4>();

    LightParam light;

    std::shared_ptr<std::vector<glm::vec3>> positions;
    std::shared_ptr<std::vector<glm::vec3>> colors;
    std::shared_ptr<std::vector<glm::uint>> indices;
    std::shared_ptr<std::vector<glm::vec2>> uvs;
    std::shared_ptr<std::vector<glm::uint>> uvIndices;
    std::shared_ptr<std::vector<glm::vec3>> norms;
    std::shared_ptr<std::vector<glm::uint>> nIndices;

    struct V2RDat {
        glm::vec4 pos;
        glm::vec4 norm, wdPos;
        union SurfDat {
            glm::vec2 uv;
            glm::vec3 col;
        } surf;
    };
    struct V2R {
        uint8_t vCnt;
        // 1 triangle generate at most 9 vertices after clipping
        std::array<V2RDat, 9> vs;
    };
    std::vector<V2R> v2rs;

    std::vector<glm::u8vec4> colorOutput;

  public:
    virtual ~RasterizerImpl() {}

    virtual void
    SetVertexData(std::shared_ptr<std::vector<glm::vec3>> positions,
                  std::shared_ptr<std::vector<glm::vec3>> colors,
                  std::shared_ptr<std::vector<glm::uint>> indices) override;
    virtual void
    SetTextureData(std::shared_ptr<std::vector<glm::vec2>> uvs,
                   std::shared_ptr<std::vector<glm::uint>> uvIndices,
                   std::shared_ptr<std::vector<glm::vec3>> norms,
                   std::shared_ptr<std::vector<glm::uint>> nIndices) override;
    virtual void SetLight(const LightParam &param) override { light = param; }
    virtual void SetModel(const glm::mat4 &model) override { M = model; }
    virtual void SetView(const glm::mat4 &view) override { V = view; }
    virtual void SetProjective(const glm::mat4 &proj) override { P = proj; }
    virtual void SetRenderSize(const glm::uvec2 &rndrSz) override;
    virtual const std::vector<glm::u8vec4> &GetColorOutput() override;

  protected:
    void runPreRasterization();
};

} // namespace kouek

#endif // !KOUEK_RAS_IMPL_H
