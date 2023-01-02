auto positions = std::make_shared<std::vector<glm::vec3>>();
auto colors = std::make_shared<std::vector<glm::vec3>>();
for (uint8_t k = 0; k < 8; ++k) {
    positions->emplace_back((k & 0x1) == 0 ? -.5f : +.5f,
                            (k & 0x2) == 0 ? -.5f : +.5f,
                            (k & 0x4) == 0 ? -.5f : +.5f);
    colors->emplace_back((k & 0x1) == 0 ? 0.f : 1.f, (k & 0x2) == 0 ? 0.f : 1.f,
                         (k & 0x4) == 0 ? 0.f : 1.f);
}
auto indices = std::make_shared<std::vector<glm::uint>>();
{
    std::array tmp{// -Z
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
    for (auto i : tmp)
        indices->emplace_back(i);
}
auto norms = std::make_shared<std::vector<glm::vec3>>();
{
    std::array tmp{// -Z
                   glm::vec3{0.f, 0.f, -1.f},
                   // +Z
                   glm::vec3{0.f, 0.f, +1.f},
                   // -Y
                   glm::vec3{0.f, -1.f, 0.f},
                   // +Y
                   glm::vec3{0.f, +1.f, 0.f},
                   // -X
                   glm::vec3{-1.f, 0.f, 0.f},
                   // +X
                   glm::vec3{+1.f, 0.f, 0.f}};
    for (auto &v : tmp)
        norms->emplace_back(v);
}
auto nIndices = std::make_shared<std::vector<glm::uint>>();
for (uint8_t k = 0; k < 12; ++k)
    for (uint8_t rep = 0; rep < 3; ++rep)
        nIndices->emplace_back(k >> 1);
