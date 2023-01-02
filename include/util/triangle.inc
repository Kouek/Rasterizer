auto positions = std::make_shared<std::vector<glm::vec3>>();
auto colors = std::make_shared<std::vector<glm::vec3>>();
for (uint8_t k = 0; k < 3; ++k) {
    positions->emplace_back((k & 0x1) == 0 ? -.5f : +.5f,
                            (k & 0x2) == 0 ? -.5f : +.5f,
                            (k & 0x4) == 0 ? -.5f : +.5f);
    colors->emplace_back((k & 0x1) == 0 ? 0.f : 1.f, (k & 0x2) == 0 ? 0.f : 1.f,
                         (k & 0x4) == 0 ? 0.f : 1.f);
}
auto indices = std::make_shared<std::vector<glm::uint>>();
{
    std::array tmp{0, 1, 2};
    for (auto i : tmp)
        indices->emplace_back(i);
}
