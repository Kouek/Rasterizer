#include <h_z_buf_ras.h>
#include <z_buf_ras.h>

#include <chrono>
#include <iostream>

#include <array>

#include <cmake_in.h>

#include <util/FPS_camera.hpp>
#include <util/mesh.hpp>

#include <cmdparser.hpp>

using namespace kouek;

static glm::uvec2 rndrSz{1024, 1024};

static FPSCamera camera({5.f, 5.f, 5.f}, glm::zero<glm::vec3>());

static std::array<std::unique_ptr<Rasterizer>, 3> rasterizers;

int main(int argc, char **argv) {
    // Command parser
    cli::Parser parser(argc, argv);
    parser.set_required<std::string>("m", "model", "Model Path");
    parser.set_optional<uint32_t>("t", "test-time", 20, "Test Repeat Times");
    parser.run_and_exit_if_error();

    // Create rasterizer
    rasterizers[0] = ZBufferRasterizer::Create();
    rasterizers[1] = SimpleHZBufferRasterizer::Create();
    rasterizers[2] = HierarchicalZBufferRasterizer::Create();
    for (auto &rasterizer : rasterizers)
        rasterizer->SetRenderSize(rndrSz);

    // Load model
    auto modelPath = parser.get<std::string>("m");
    auto positions = std::make_shared<std::vector<glm::vec3>>();
    auto indices = std::make_shared<std::vector<glm::uint>>();
    try {
        kouek::Mesh mesh;
        mesh.ReadFromFile(modelPath);
        auto &vs = mesh.GetVS();
        auto &fvs = mesh.GetFVS();
        auto &vts = mesh.GetVTS();
        auto &fvts = mesh.GetFVTS();
        auto &vns = mesh.GetVNS();
        auto &fvns = mesh.GetFVNS();
        std::cout << "Model: " << modelPath << std::endl;
        std::cout << ">> Model Vertices Num: " << vs.size() << std::endl;
        std::cout << ">> Model Faces Num: " << fvs.size() << std::endl;

        (*positions) = vs;
        indices->reserve(fvs.size() * 3);
        for (const auto &idx3 : fvs)
            for (uint8_t t = 0; t < 3; ++t)
                indices->emplace_back(idx3[t]);
        for (auto &rasterizer : rasterizers)
            rasterizer->SetVertexData(positions, nullptr, indices);

        std::shared_ptr<std::vector<glm::vec2>> uvs;
        std::shared_ptr<std::vector<glm::uint>> uvIndices;
        std::shared_ptr<std::vector<glm::vec3>> norms;
        std::shared_ptr<std::vector<glm::uint>> nIndices;
        if (!fvts.empty()) {
            uvs = std::make_shared<std::vector<glm::vec2>>();
            uvIndices = std::make_shared<std::vector<glm::uint>>();
            (*uvs) = vts;
            for (const auto &idx3 : fvts)
                for (uint8_t t = 0; t < 3; ++t)
                    uvIndices->emplace_back(idx3[t]);
        }
        if (!fvns.empty()) {
            norms = std::make_shared<std::vector<glm::vec3>>();
            nIndices = std::make_shared<std::vector<glm::uint>>();
            (*norms) = vns;
            for (const auto &idx3 : fvns)
                for (uint8_t t = 0; t < 3; ++t)
                    nIndices->emplace_back(idx3[t]);
        }
        for (auto &rasterizer : rasterizers)
            rasterizer->SetTextureData(uvs, uvIndices, norms, nIndices);

    } catch (std::exception &exp) {
        std::cout << "Model: " << modelPath << "loading failed." << std::endl;
        std::cout << ">> Error: " << exp.what() << std::endl;
    }

    // Render Config
    for (auto &rasterizer : rasterizers) {
        rasterizer->SetModel(glm::identity<glm::mat4>());
        rasterizer->SetProjective(glm::perspectiveFov(
            glm::radians(60.f), (float)rndrSz.x, (float)rndrSz.y, .01f, 10.f));
        rasterizer->SetView(camera.GetViewMat());
    }
    {
        Rasterizer::LightParam param;
        param.ambientStrength = .1f;
        param.ambientColor = glm::vec3{1.f, 1.f, 1.f};
        param.position = glm::vec3{0.f, 5.f, 0.f};
        param.color = glm::vec3{.6f, .5f, .8f};
        for (auto &rasterizer : rasterizers)
            rasterizer->SetLight(param);
    }

    auto repeatTimes = parser.get<uint32_t>("t");
    auto t0 = std::chrono::system_clock::now();
    for (uint32_t rpt = 0; rpt < repeatTimes; ++rpt)
        rasterizers[0]->Render();
    auto t1 = std::chrono::system_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    std::cout << "Rasterizer: "
              << "Normal" << std::endl;
    std::cout << ">> Avg Duration (" << repeatTimes
              << " times): " << ((double)duration.count() / repeatTimes)
              << " ms" << std::endl;

    t0 = std::chrono::system_clock::now();
    for (uint32_t rpt = 0; rpt < repeatTimes; ++rpt)
        rasterizers[1]->Render();
    t1 = std::chrono::system_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    std::cout << "Rasterizer: "
              << "Simple Hierarchical ZBuffer" << std::endl;
    std::cout << ">> Avg Duration (" << repeatTimes
              << " times): " << ((double)duration.count() / repeatTimes)
              << " ms" << std::endl;

    t0 = std::chrono::system_clock::now();
    for (uint32_t rpt = 0; rpt < repeatTimes; ++rpt)
        rasterizers[2]->Render();
    t1 = std::chrono::system_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    std::cout << "Rasterizer: "
              << "Hierarchical ZBuffer" << std::endl;
    std::cout << ">> Avg Duration (" << repeatTimes
              << " times): " << ((double)duration.count() / repeatTimes)
              << " ms" << std::endl;

    return 0;
}
