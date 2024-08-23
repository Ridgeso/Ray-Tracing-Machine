#include "GlfwWindow.h"

#include "Engine/Render/RenderApi.h"
#include "Engine/Event/Event.h"
#include "Engine/Event/AppEvents.h"

#include <imgui.h>
#include "External/window/ImGuiImpl.h"

namespace
{
    
    const char* glfwError2String(const int32_t error)
    {
        switch (error)
        {
            case GLFW_NO_ERROR:            return "GLFW_NO_ERROR";
            case GLFW_NOT_INITIALIZED:     return "GLFW_NOT_INITIALIZED";
            case GLFW_NO_CURRENT_CONTEXT:  return "GLFW_NO_CURRENT_CONTEXT";
            case GLFW_INVALID_ENUM:        return "GLFW_INVALID_ENUM";
            case GLFW_INVALID_VALUE:       return "GLFW_INVALID_VALUE";
            case GLFW_OUT_OF_MEMORY:       return "GLFW_OUT_OF_MEMORY";
            case GLFW_API_UNAVAILABLE:     return "GLFW_API_UNAVAILABLE";
            case GLFW_VERSION_UNAVAILABLE: return "GLFW_VERSION_UNAVAILABLE";
            case GLFW_PLATFORM_ERROR:      return "GLFW_PLATFORM_ERROR";
            case GLFW_FORMAT_UNAVAILABLE:  return "GLFW_FORMAT_UNAVAILABLE";
            case GLFW_NO_WINDOW_CONTEXT:   return "GLFW_NO_WINDOW_CONTEXT";
        }
        return "not found";
    }

    void GLFWErrorCallback(int32_t error, const char* description)
    {
        RT_LOG_ERROR("GLFW Callback {}: {}", glfwError2String(error), description);
    }

    void closeWindow(GLFWwindow* /*window*/)
    {
        auto event = RT::Event::Event<RT::Event::AppClose>{};
        event.process();
    }
    
    void windowResize(GLFWwindow* window, int32_t width, int32_t height)
    {
        auto& data = *(RT::GlfwWindow::Context*)glfwGetWindowUserPointer(window);
        data.size = { width, height };
        data.isMinimized = 0 == width && 0 == height;

        auto event = RT::Event::Event<RT::Event::WindowResize>{};
        event.fill([&data](auto& e)
        {
            e.width = data.size.x;
            e.height = data.size.y;
            e.isMinimized = data.isMinimized;
        });
        event.process();
    }
    
    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        auto event = RT::Event::Event<RT::Event::KeyPressed>{};
        event.fill([key, action, mods](auto& e)
        {
            e.key = static_cast<RT::Keys::Keyboard>(key);
            e.action = static_cast<RT::Keys::Action>(action);
            e.mod = static_cast<RT::Keys::KeyCode>(mods);
        });
        event.process();
    }

    void mouseCallback(GLFWwindow* window, int32_t button, int32_t action, int32_t mods)
    {
        auto event = RT::Event::Event<RT::Event::MousePressed>{};
        event.fill([button, action, mods](auto& e)
        {
            e.button = static_cast<RT::Keys::Keyboard>(button);
            e.action = static_cast<RT::Keys::Action>(action);
            e.mod = static_cast<RT::Keys::KeyCode>(mods);
        });
        event.process();
    }

    void cursorPositionCallback(GLFWwindow* window, double xPos, double yPos)
    {
        auto event = RT::Event::Event<RT::Event::MouseMove>{};
        event.fill([xPos, yPos](auto& e)
        {
            e.xPos = static_cast<int32_t>(xPos);
            e.yPos = static_cast<int32_t>(yPos);
        });
        event.process();
    }

    void scrollCallback(GLFWwindow* window, double xOffset, double yOffset)
    {
        auto event = RT::Event::Event<RT::Event::ScrollMoved>{};
        event.fill([xOffset, yOffset](auto& e)
        {
            e.xOffset = static_cast<int32_t>(xOffset);
            e.yOffset = static_cast<int32_t>(yOffset);
        });
        event.process();
    }

}

namespace RT
{

    void GlfwWindow::init(const WindowSpecs& specs)
    {
        if (!glfwInit())
        {
            return;
        }
        if (RenderApi::api == RenderApi::Api::Vulkan)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        }

        context.title = specs.titel;
        context.size.x = specs.width;
        context.size.y = specs.height;
        context.isMinimized = specs.isMinimized;

        window = glfwCreateWindow(context.size.x, context.size.y, context.title.c_str(), NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            return;
        }
        glfwSetWindowUserPointer(window, &context);

        glfwMakeContextCurrent(window);
        glfwSwapInterval(0);

        setCallbacks();

        initImGui();
    }

    void GlfwWindow::shutDown()
    {
        ImGuiImpl::shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void GlfwWindow::setTitleBar(const std::string& title)
    {
        glfwSetWindowTitle(window, title.c_str());
    }

    void GlfwWindow::update()
    {
        glfwPollEvents();
        //glfwSwapBuffers(window);
    }

    void GlfwWindow::beginUI()
    {
        ImGuiImpl::begin();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    }

    void GlfwWindow::endUI()
    {
        ImGui::Render();
        ImGuiImpl::end();

        ImGuiIO& io = ImGui::GetIO(); (void)io;
        
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backupCurrentContext);
        }
    }

    glm::vec2 GlfwWindow::getMousePos() const
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        return { (float)x, (float)y };
    }

    bool GlfwWindow::isKeyPressed(const Keys::Keyboard key) const
    {
        return glfwGetKey(window, static_cast<int32_t>(key)) == GLFW_PRESS;
    }

    bool GlfwWindow::isMousePressed(const Keys::Mouse button) const
    {
        return glfwGetMouseButton(window, static_cast<int32_t>(button)) == GLFW_PRESS;
    }

    glm::ivec2 GlfwWindow::getSize() const
    {
        int32_t width, height;
        glfwGetWindowSize(window, &width, &height);
        return { width, height };
    }

    void GlfwWindow::cursorMode(const Keys::MouseMod mod) const
    {
        glfwSetInputMode(window, GLFW_CURSOR, static_cast<int32_t>(mod));
    }

    void GlfwWindow::setCallbacks()
    {
        glfwSetErrorCallback(GLFWErrorCallback);
        glfwSetWindowCloseCallback(window, closeWindow);
        glfwSetWindowSizeCallback(window, windowResize);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetMouseButtonCallback(window, mouseCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetCursorPosCallback(window, cursorPositionCallback);
    }

    void GlfwWindow::initImGui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        auto& io = ImGui::GetIO(); (void)io;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard
            | ImGuiConfigFlags_DockingEnable
            | ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsClassic();

        auto& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        ImGuiImpl::initImGui(window);
    }

}

#define CHECK_KEY_COMPATIBILITY(GLFW_KEY_CODE, RT_KEY_CODE)                                           \
    static_assert(                                                                                    \
        static_cast<RT::Keys::KeyCode>(GLFW_KEY_CODE) == static_cast<RT::Keys::KeyCode>(RT_KEY_CODE), \
        "GLFW keys are not longer compatible with RT key codes");

CHECK_KEY_COMPATIBILITY(GLFW_PRESS,   RT::Keys::Action::Press)
CHECK_KEY_COMPATIBILITY(GLFW_REPEAT,  RT::Keys::Action::Repeat)
CHECK_KEY_COMPATIBILITY(GLFW_RELEASE, RT::Keys::Action::Release)

CHECK_KEY_COMPATIBILITY(GLFW_MOD_SHIFT,   RT::Keys::Mod::Shift)
CHECK_KEY_COMPATIBILITY(GLFW_MOD_CONTROL, RT::Keys::Mod::Control)
CHECK_KEY_COMPATIBILITY(GLFW_MOD_ALT,     RT::Keys::Mod::Alt)
CHECK_KEY_COMPATIBILITY(GLFW_MOD_SUPER,   RT::Keys::Mod::Super)

CHECK_KEY_COMPATIBILITY(GLFW_MOUSE_BUTTON_1, RT::Keys::Mouse::Button1)
CHECK_KEY_COMPATIBILITY(GLFW_MOUSE_BUTTON_2, RT::Keys::Mouse::Button2)
CHECK_KEY_COMPATIBILITY(GLFW_MOUSE_BUTTON_3, RT::Keys::Mouse::Button3)
CHECK_KEY_COMPATIBILITY(GLFW_MOUSE_BUTTON_4, RT::Keys::Mouse::Button4)
CHECK_KEY_COMPATIBILITY(GLFW_MOUSE_BUTTON_5, RT::Keys::Mouse::Button5)
CHECK_KEY_COMPATIBILITY(GLFW_MOUSE_BUTTON_6, RT::Keys::Mouse::Button6)
CHECK_KEY_COMPATIBILITY(GLFW_MOUSE_BUTTON_7, RT::Keys::Mouse::Button7)
CHECK_KEY_COMPATIBILITY(GLFW_MOUSE_BUTTON_8, RT::Keys::Mouse::Button8)
CHECK_KEY_COMPATIBILITY(GLFW_MOUSE_BUTTON_LAST,   RT::Keys::Mouse::ButtonLast)
CHECK_KEY_COMPATIBILITY(GLFW_MOUSE_BUTTON_LEFT,   RT::Keys::Mouse::ButtonLeft)
CHECK_KEY_COMPATIBILITY(GLFW_MOUSE_BUTTON_RIGHT,  RT::Keys::Mouse::ButtonRight)
CHECK_KEY_COMPATIBILITY(GLFW_MOUSE_BUTTON_MIDDLE, RT::Keys::Mouse::ButtonMiddle)

CHECK_KEY_COMPATIBILITY(GLFW_CURSOR_NORMAL,   RT::Keys::MouseMod::Normal)
CHECK_KEY_COMPATIBILITY(GLFW_CURSOR_HIDDEN,   RT::Keys::MouseMod::Hidden)
CHECK_KEY_COMPATIBILITY(GLFW_CURSOR_DISABLED, RT::Keys::MouseMod::Disabled)
CHECK_KEY_COMPATIBILITY(GLFW_CURSOR_CAPTURED, RT::Keys::MouseMod::Captured)

CHECK_KEY_COMPATIBILITY(GLFW_KEY_0, RT::Keys::Keyboard::_0)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_1, RT::Keys::Keyboard::_1)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_2, RT::Keys::Keyboard::_2)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_3, RT::Keys::Keyboard::_3)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_4, RT::Keys::Keyboard::_4)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_5, RT::Keys::Keyboard::_5)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_6, RT::Keys::Keyboard::_6)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_7, RT::Keys::Keyboard::_7)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_8, RT::Keys::Keyboard::_8)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_9, RT::Keys::Keyboard::_9)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_0, RT::Keys::Keyboard::_0)

CHECK_KEY_COMPATIBILITY(GLFW_KEY_SEMICOLON, RT::Keys::Keyboard::Semicolon)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_EQUAL,     RT::Keys::Keyboard::Equal)

CHECK_KEY_COMPATIBILITY(GLFW_KEY_A, RT::Keys::Keyboard::A)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_B, RT::Keys::Keyboard::B)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_C, RT::Keys::Keyboard::C)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_D, RT::Keys::Keyboard::D)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_E, RT::Keys::Keyboard::E)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F, RT::Keys::Keyboard::F)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_G, RT::Keys::Keyboard::G)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_H, RT::Keys::Keyboard::H)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_I, RT::Keys::Keyboard::I)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_J, RT::Keys::Keyboard::J)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_K, RT::Keys::Keyboard::K)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_L, RT::Keys::Keyboard::L)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_M, RT::Keys::Keyboard::M)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_N, RT::Keys::Keyboard::N)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_O, RT::Keys::Keyboard::O)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_P, RT::Keys::Keyboard::P)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_Q, RT::Keys::Keyboard::Q)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_R, RT::Keys::Keyboard::R)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_S, RT::Keys::Keyboard::S)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_T, RT::Keys::Keyboard::T)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_U, RT::Keys::Keyboard::U)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_V, RT::Keys::Keyboard::V)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_W, RT::Keys::Keyboard::W)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_X, RT::Keys::Keyboard::X)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_Y, RT::Keys::Keyboard::Y)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_Z, RT::Keys::Keyboard::Z)

CHECK_KEY_COMPATIBILITY(GLFW_KEY_LEFT_BRACKET,  RT::Keys::Keyboard::LeftBracket)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_BACKSLASH,     RT::Keys::Keyboard::Backslash)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_RIGHT_BRACKET, RT::Keys::Keyboard::RightBracket)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_GRAVE_ACCENT,  RT::Keys::Keyboard::GraveAccent)

CHECK_KEY_COMPATIBILITY(GLFW_KEY_WORLD_1, RT::Keys::Keyboard::World1)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_WORLD_2, RT::Keys::Keyboard::World2)

CHECK_KEY_COMPATIBILITY(GLFW_KEY_ENTER,        RT::Keys::Keyboard::Enter)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_ESCAPE,       RT::Keys::Keyboard::Escape)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_TAB,          RT::Keys::Keyboard::Tab)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_BACKSPACE,    RT::Keys::Keyboard::Backspace)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_INSERT,       RT::Keys::Keyboard::Insert)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_DELETE,       RT::Keys::Keyboard::Delete)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_RIGHT,        RT::Keys::Keyboard::Right)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_LEFT,         RT::Keys::Keyboard::Left)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_DOWN,         RT::Keys::Keyboard::Down)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_UP,           RT::Keys::Keyboard::Up)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_PAGE_UP,      RT::Keys::Keyboard::PageUp)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_PAGE_DOWN,    RT::Keys::Keyboard::PageDown)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_HOME,         RT::Keys::Keyboard::Home)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_END,          RT::Keys::Keyboard::End)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_CAPS_LOCK,    RT::Keys::Keyboard::CapsLock)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_SCROLL_LOCK,  RT::Keys::Keyboard::ScrollLock)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_NUM_LOCK,     RT::Keys::Keyboard::NumLock)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_PRINT_SCREEN, RT::Keys::Keyboard::PrintScreen)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_PAUSE,        RT::Keys::Keyboard::Pause)

CHECK_KEY_COMPATIBILITY(GLFW_KEY_F1,  RT::Keys::Keyboard::F1)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F2,  RT::Keys::Keyboard::F2)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F3,  RT::Keys::Keyboard::F3)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F4,  RT::Keys::Keyboard::F4)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F5,  RT::Keys::Keyboard::F5)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F6,  RT::Keys::Keyboard::F6)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F7,  RT::Keys::Keyboard::F7)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F8,  RT::Keys::Keyboard::F8)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F9,  RT::Keys::Keyboard::F9)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F10, RT::Keys::Keyboard::F10)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F11, RT::Keys::Keyboard::F11)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F12, RT::Keys::Keyboard::F12)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F13, RT::Keys::Keyboard::F13)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F14, RT::Keys::Keyboard::F14)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F15, RT::Keys::Keyboard::F15)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F16, RT::Keys::Keyboard::F16)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F17, RT::Keys::Keyboard::F17)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F18, RT::Keys::Keyboard::F18)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F19, RT::Keys::Keyboard::F19)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F20, RT::Keys::Keyboard::F20)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F21, RT::Keys::Keyboard::F21)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F22, RT::Keys::Keyboard::F22)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F23, RT::Keys::Keyboard::F23)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F24, RT::Keys::Keyboard::F24)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_F25, RT::Keys::Keyboard::F25)

CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_0, RT::Keys::Keyboard::KP0)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_1, RT::Keys::Keyboard::KP1)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_2, RT::Keys::Keyboard::KP2)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_3, RT::Keys::Keyboard::KP3)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_4, RT::Keys::Keyboard::KP4)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_5, RT::Keys::Keyboard::KP5)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_6, RT::Keys::Keyboard::KP6)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_7, RT::Keys::Keyboard::KP7)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_8, RT::Keys::Keyboard::KP8)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_9, RT::Keys::Keyboard::KP9)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_DECIMAL,  RT::Keys::Keyboard::KPDecimal)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_DIVIDE,   RT::Keys::Keyboard::KPDivide)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_MULTIPLY, RT::Keys::Keyboard::KPMultiply)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_SUBTRACT, RT::Keys::Keyboard::KPSubtract)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_ADD,      RT::Keys::Keyboard::KPAdd)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_ENTER,    RT::Keys::Keyboard::KPEnter)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_KP_EQUAL,    RT::Keys::Keyboard::KPEqual)

CHECK_KEY_COMPATIBILITY(GLFW_KEY_LEFT_SHIFT,    RT::Keys::Keyboard::LeftShift)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_LEFT_CONTROL,  RT::Keys::Keyboard::LeftControl)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_LEFT_ALT,      RT::Keys::Keyboard::LeftAlt)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_LEFT_SUPER,    RT::Keys::Keyboard::LeftSuper)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_RIGHT_SHIFT,   RT::Keys::Keyboard::RightShift)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_RIGHT_CONTROL, RT::Keys::Keyboard::RightControl)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_RIGHT_ALT,     RT::Keys::Keyboard::RightAlt)
CHECK_KEY_COMPATIBILITY(GLFW_KEY_RIGHT_SUPER,   RT::Keys::Keyboard::RightSuper)

#undef CHECK_KEY_COMPATIBILITY
