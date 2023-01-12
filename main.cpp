#include <h_z_buf_ras.h>
#include <z_buf_ras.h>

#include <iostream>

#include <array>

#include <glad/glad.h>

#include <glfw/glfw3.h>

#include <cmake_in.h>

#include <util/FPS_camera.hpp>
#include <util/mesh.hpp>
#include <util/shader.hpp>

#include <cmdparser.hpp>

using namespace kouek;

static glm::uvec2 rndrSz{1024, 1024};

static GLuint rndrTex;

static FPSCamera camera({5.f, 5.f, 5.f}, glm::zero<glm::vec3>());

static std::unique_ptr<Rasterizer> rasterizer;

int main(int argc, char **argv) {
    // Command parser
    cli::Parser parser(argc, argv);
    parser.set_required<std::string>("m", "model", "Model Path");
    parser.set_optional<uint32_t>(
        "r", "rasterizer", 1,
        "Rasterizer Type, 0: Normal, 1: Hierarchical, 2: Simple Hierarchical");
    parser.run_and_exit_if_error();

    // GLFW context
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window =
        glfwCreateWindow(rndrSz.x, rndrSz.y, "Rasterizer", nullptr, nullptr);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    auto onKeyActivated = [](GLFWwindow *window, int key, int scancode,
                             int action, int mods) {
        static constexpr auto ROT_SENS = 1.f;
        static constexpr auto MOV_SENS = .05f;

        switch (key) {
        case GLFW_KEY_ESCAPE:
            if (action == GLFW_RELEASE)
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_UP:
            camera.Revolve(
                glm::distance(camera.GetPos(), glm::zero<glm::vec3>()), 0.f,
                +ROT_SENS);
            break;
        case GLFW_KEY_DOWN:
            camera.Revolve(
                glm::distance(camera.GetPos(), glm::zero<glm::vec3>()), 0.f,
                -ROT_SENS);
            break;
        case GLFW_KEY_LEFT:
            camera.Revolve(
                glm::distance(camera.GetPos(), glm::zero<glm::vec3>()),
                +ROT_SENS, 0.f);
            break;
        case GLFW_KEY_RIGHT:
            camera.Revolve(
                glm::distance(camera.GetPos(), glm::zero<glm::vec3>()),
                -ROT_SENS, 0.f);
            break;
        case GLFW_KEY_Q:
            camera.Move(0.f, 0.f, -MOV_SENS);
            break;
        case GLFW_KEY_E:
            camera.Move(0.f, 0.f, +MOV_SENS);
            break;
        default:
            break;
        }

        switch (key) {
        case GLFW_KEY_UP:
        case GLFW_KEY_DOWN:
        case GLFW_KEY_LEFT:
        case GLFW_KEY_RIGHT:
        case GLFW_KEY_Q:
        case GLFW_KEY_E:
            rasterizer->SetView(camera.GetViewMat());
            break;
        default:
            break;
        }
    };
    glfwSetKeyCallback(window, onKeyActivated);

    // Load OpenGL functions
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Create rasterizer
    switch (parser.get<uint32_t>("r")) {
    case 0:
        std::cout << "Rasterizer: "
                  << "Normal" << std::endl;
        rasterizer = ZBufferRasterizer::Create();
        break;
    case 1:
        std::cout << "Rasterizer: "
                  << "Hierarchical ZBuffer" << std::endl;
        rasterizer = HierarchicalZBufferRasterizer::Create();
        break;
    case 2:
        std::cout << "Rasterizer: "
                  << "Simple Hierarchical ZBuffer" << std::endl;
        rasterizer = SimpleHZBufferRasterizer::Create();
        break;
    }
    rasterizer->SetRenderSize(rndrSz);

    // Create texture to be rendered in GL
    glGenTextures(1, &rndrTex);
    glBindTexture(GL_TEXTURE_2D, rndrTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rndrSz.x, rndrSz.y, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, (const void *)0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // GL drawing resources
    GLuint VBO = 0, VAO = 0, EBO = 0;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    {
        std::vector<GLfloat> verts = {-1.f, -1.f, 0, 0,   1.f, -1.f, 1.f, 0,
                                      -1.f, 1.f,  0, 1.f, 1.f, 1.f,  1.f, 1.f};
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * verts.size(),
                     verts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4,
                              (const void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4,
                              (const void *)(sizeof(GLfloat) * 2));
    }
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    {
        GLushort idxes[] = {0, 1, 3, 0, 3, 2};
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * 6, idxes,
                     GL_STATIC_DRAW);
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    Shader screenQuadShader(
        (std::string(PROJECT_SOURCE_DIR) + "/screen_quad.vs").c_str(),
        (std::string(PROJECT_SOURCE_DIR) + "/screen_quad.fs").c_str());

    // Load model

//#define TEST_CUBE
//#define TEST_TRIANGLE
#ifdef TEST_CUBE
#include <util/cube.inc>
    rasterizer->SetVertexData(positions, colors, indices);
    rasterizer->SetTextureData(nullptr, nullptr, norms, nIndices);
#elif defined(TEST_TRIANGLE)
#include <util/triangle.inc>
    rasterizer->SetVertexData(positions, colors, indices);
#else
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
        rasterizer->SetTextureData(uvs, uvIndices, norms, nIndices);

    } catch (std::exception &exp) {
        std::cout << "Model: " << modelPath << "loading failed." << std::endl;
        std::cout << ">> Error: " << exp.what() << std::endl;
    }
#endif // TEST_CUBE

    // Render Loop
    rasterizer->SetModel(glm::identity<glm::mat4>());
    rasterizer->SetProjective(glm::perspectiveFov(
        glm::radians(60.f), (float)rndrSz.x, (float)rndrSz.y, .01f, 10.f));
    rasterizer->SetView(camera.GetViewMat());
    {
        Rasterizer::LightParam param;
        param.ambientStrength = .1f;
        param.ambientColor = glm::vec3{1.f, 1.f, 1.f};
        param.position = glm::vec3{5.f, 5.f, 5.f};
        param.color = glm::vec3{.6f, .5f, .8f};
        rasterizer->SetLight(param);
    }

    screenQuadShader.use();
    glClearColor(0, 0, 0, 1.f);
    glBindTexture(GL_TEXTURE_2D, rndrTex);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        rasterizer->Render();
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rndrSz.x, rndrSz.y, GL_RGBA,
                        GL_UNSIGNED_BYTE, rasterizer->GetColorOutput().data());

        glClear(GL_COLOR_BUFFER_BIT);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const void *)0);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteTextures(1, &rndrTex);
    glfwTerminate();

    return 0;
}
