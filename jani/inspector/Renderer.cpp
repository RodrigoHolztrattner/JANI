#include "Renderer.h"

Jani::Inspector::Renderer::Renderer()
{

}

Jani::Inspector::Renderer::~Renderer()
{
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

bool Jani::Inspector::Renderer::Initialize(uint32_t _window_width, uint32_t _window_height)
{
    // Setup m_window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    m_window = glfwCreateWindow(_window_width, _window_height, "JANI Inspector", NULL, NULL);
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync

    gladLoadGL((GLADloadfunc)glfwGetProcAddress);

    //gl3wInit();


    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init();

    // Setup style
    //ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    ImGui::StyleColorsLight();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);

    // auto font_default = io.Fonts->AddFontDefault();
    // auto font_cousine = io.Fonts->AddFontFromFileTTF( CPP_SRC_DIR "modules/imgui/misc/fonts/Cousine-Regular.ttf", 15.0f);
    // auto font_karla   = io.Fonts->AddFontFromFileTTF( CPP_SRC_DIR "modules/imgui/misc/fonts/Karla-Regular.ttf", 18.0f);
    io.Fonts->AddFontFromFileTTF(CPP_SRC_DIR "jani/inspector/fonts/Lato-Regular.ttf", 18.0f);

    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    ///////////
    // THEME //
    ///////////

    ImGuiStyle* style = &ImGui::GetStyle();

    int hspacing = 8;
    int vspacing = 6;
    style->DisplaySafeAreaPadding = ImVec2(0, 0);
    style->WindowPadding = ImVec2(hspacing/2, vspacing);
    style->FramePadding = ImVec2(hspacing, vspacing);
    style->ItemSpacing = ImVec2(hspacing, vspacing);
    style->ItemInnerSpacing = ImVec2(hspacing, vspacing);
    style->IndentSpacing = 20.0f;

    style->WindowRounding = 0.0f;
    style->FrameRounding = 0.0f;

    style->WindowBorderSize = 0.0f;
    style->FrameBorderSize = 1.0f;
    style->PopupBorderSize = 1.0f;

    style->ScrollbarSize = 20.0f;
    style->ScrollbarRounding = 0.0f;
    style->GrabMinSize = 5.0f;
    style->GrabRounding = 0.0f;

    ImVec4 white = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    ImVec4 transparent = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    ImVec4 dark = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
    ImVec4 darker = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);

    ImVec4 background = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    ImVec4 text = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    ImVec4 border = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    ImVec4 grab = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
    ImVec4 header = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    ImVec4 active = ImVec4(0.00f, 0.47f, 0.84f, 1.00f);
    ImVec4 hover = ImVec4(0.00f, 0.47f, 0.84f, 0.20f);

    style->Colors[ImGuiCol_Text] = text;
    style->Colors[ImGuiCol_WindowBg] = background;
    style->Colors[ImGuiCol_ChildBg] = background;
    style->Colors[ImGuiCol_PopupBg] = white;

    style->Colors[ImGuiCol_Border] = border;
    style->Colors[ImGuiCol_BorderShadow] = transparent;

    style->Colors[ImGuiCol_Button] = header;
    style->Colors[ImGuiCol_ButtonHovered] = hover;
    style->Colors[ImGuiCol_ButtonActive] = active;

    style->Colors[ImGuiCol_FrameBg] = white;
    style->Colors[ImGuiCol_FrameBgHovered] = hover;
    style->Colors[ImGuiCol_FrameBgActive] = active;

    style->Colors[ImGuiCol_MenuBarBg] = header;
    style->Colors[ImGuiCol_Header] = header;
    style->Colors[ImGuiCol_HeaderHovered] = hover;
    style->Colors[ImGuiCol_HeaderActive] = active;

    style->Colors[ImGuiCol_CheckMark] = text;
    style->Colors[ImGuiCol_SliderGrab] = grab;
    style->Colors[ImGuiCol_SliderGrabActive] = darker;

    //style->Colors[ImGuiCol_CloseButton] = transparent;
    //style->Colors[ImGuiCol_CloseButtonHovered] = transparent;
    //style->Colors[ImGuiCol_CloseButtonActive] = transparent;

    style->Colors[ImGuiCol_ScrollbarBg] = header;
    style->Colors[ImGuiCol_ScrollbarGrab] = grab;
    style->Colors[ImGuiCol_ScrollbarGrabHovered] = dark;
    style->Colors[ImGuiCol_ScrollbarGrabActive] = darker;


    return true;
}

bool Jani::Inspector::Renderer::BeginRenderFrame()
{
    if (glfwWindowShouldClose(m_window))
    {
        return false;
    }

    // Poll and handle events (inputs, m_window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    glfwPollEvents();

    // Start the ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    return true;
}

void Jani::Inspector::Renderer::EndRenderFrame()
{
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwMakeContextCurrent(m_window);
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwMakeContextCurrent(m_window);
    glfwSwapBuffers(m_window);
}

int main2(int, char**)
{
    bool show_demo_window = true;
    bool show_theme_window = true;
    bool show_another_window = false;

    // 1. Show a simple m_window.
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets automatically appears in a m_window called "Debug".
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Text("Windows");
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our windows open/close state
        ImGui::Checkbox("Themes Window", &show_theme_window);      // Edit bools storing our windows open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::Text("Font Render Test - Lato: Bit Test.123");
        ImGui::Text("Font Render Test - Lato: XXXXXXXXXXXX");

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (NB: most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    }

    return 0;
}
