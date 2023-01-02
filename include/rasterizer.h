#ifndef KOUEK_RAS_H
#define KOUEK_RAS_H

#include <memory>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

namespace kouek {

class Rasterizer {
  public:
    virtual ~Rasterizer() {}

    virtual void Render() = 0;
    virtual void
    SetVertexData(std::shared_ptr<std::vector<glm::vec3>> positions,
                  std::shared_ptr<std::vector<glm::uint>> indices,
                  std::shared_ptr<std::vector<glm::vec2>> uvs,
                  std::shared_ptr<std::vector<glm::uint>> uvIndices) = 0;
    virtual void
    SetVertexData(std::shared_ptr<std::vector<glm::vec3>> positions,
                  std::shared_ptr<std::vector<glm::vec3>> colors,
                  std::shared_ptr<std::vector<glm::uint>> indices) = 0;
    virtual void SetModel(const glm::mat4 &model) = 0;
    virtual void SetView(const glm::mat4 &view) = 0;
    virtual void SetProjective(const glm::mat4 &proj) = 0;
    virtual void SetRenderSize(const glm::uvec2 &rndrSz) = 0;
    virtual const std::vector<glm::u8vec4> &GetColorOutput() = 0;
};

} // namespace kouek

#endif // !KOUEK_RAS_H
