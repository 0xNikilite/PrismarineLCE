#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "launcher.h"
#include <cstdio>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct SkinTexture {
    GLuint tex = 0;
    int w = 0, h = 0;
    std::string path;
};

static std::vector<SkinTexture> g_skin_textures;

static ImFont* g_font_sm = nullptr;
static ImFont* g_font_md = nullptr;
static ImFont* g_font_lg = nullptr;
static ImFont* g_font_xl = nullptr;

static GLuint g_dirt_texture = 0;
static int g_dirt_w = 0, g_dirt_h = 0;

struct MCColor {
    static ImVec4 Stone()      { return ImVec4(0.43f, 0.43f, 0.43f, 1.0f); }
    static ImVec4 StoneLight() { return ImVec4(0.53f, 0.53f, 0.53f, 1.0f); }
    static ImVec4 StoneDark()  { return ImVec4(0.33f, 0.33f, 0.33f, 1.0f); }
    static ImVec4 Dirt()       { return ImVec4(0.30f, 0.22f, 0.14f, 1.0f); }
    static ImVec4 DirtDark()   { return ImVec4(0.20f, 0.15f, 0.09f, 1.0f); }
    static ImVec4 Green()      { return ImVec4(0.33f, 0.66f, 0.20f, 1.0f); }
    static ImVec4 GreenDark()  { return ImVec4(0.23f, 0.50f, 0.14f, 1.0f); }
    static ImVec4 Yellow()     { return ImVec4(1.0f, 1.0f, 0.63f, 1.0f); }
    static ImVec4 White()      { return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); }
    static ImVec4 Gray()       { return ImVec4(0.63f, 0.63f, 0.63f, 1.0f); }
    static ImVec4 DarkGray()   { return ImVec4(0.25f, 0.25f, 0.25f, 1.0f); }
    static ImVec4 Black()      { return ImVec4(0.0f, 0.0f, 0.0f, 1.0f); }
    static ImVec4 Shadow()     { return ImVec4(0.15f, 0.15f, 0.15f, 1.0f); }
    static ImVec4 BgDark()     { return ImVec4(0.12f, 0.10f, 0.10f, 1.0f); }
    static ImVec4 BgPanel()    { return ImVec4(0.16f, 0.14f, 0.14f, 0.95f); }
};

static ImU32 ToU32(ImVec4 c) {
    return IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), (int)(c.w*255));
}

static void ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = s.ChildRounding = s.FrameRounding = 0.0f;
    s.PopupRounding = s.ScrollbarRounding = s.GrabRounding = s.TabRounding = 0.0f;
    s.WindowBorderSize = s.FrameBorderSize = 0.0f;
    s.FramePadding  = ImVec2(8, 6);
    s.ItemSpacing   = ImVec2(8, 6);
    s.WindowPadding = ImVec2(0, 0);
    s.ScrollbarSize = 14.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = MCColor::BgDark();
    c[ImGuiCol_ChildBg]              = ImVec4(0,0,0,0);
    c[ImGuiCol_PopupBg]              = MCColor::BgPanel();
    c[ImGuiCol_Border]               = MCColor::Shadow();
    c[ImGuiCol_Text]                 = MCColor::White();
    c[ImGuiCol_TextDisabled]         = MCColor::DarkGray();
    c[ImGuiCol_Button]               = MCColor::Stone();
    c[ImGuiCol_ButtonHovered]        = MCColor::StoneLight();
    c[ImGuiCol_ButtonActive]         = MCColor::StoneDark();
    c[ImGuiCol_FrameBg]              = MCColor::DarkGray();
    c[ImGuiCol_FrameBgHovered]       = MCColor::Stone();
    c[ImGuiCol_FrameBgActive]        = MCColor::StoneDark();
    c[ImGuiCol_ScrollbarBg]          = MCColor::Shadow();
    c[ImGuiCol_ScrollbarGrab]        = MCColor::Stone();
    c[ImGuiCol_ScrollbarGrabHovered] = MCColor::StoneLight();
    c[ImGuiCol_Header]               = MCColor::Stone();
    c[ImGuiCol_HeaderHovered]        = MCColor::StoneLight();
    c[ImGuiCol_Separator]            = MCColor::Shadow();
}

static bool LoadTextureFromFile(const char* filename, GLuint* out_tex, int* out_w, int* out_h) {
    int iw = 0, ih = 0;
    unsigned char* data = stbi_load(filename, &iw, &ih, NULL, 4);
    if (!data) return false;
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, iw, ih, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    *out_tex = tex; *out_w = iw; *out_h = ih;
    return true;
}

static void EnsureSkinTextureLoaded(const std::string& path) {
    for (auto& st : g_skin_textures) if (st.path == path) return;
    GLuint tex; int w, h;
    if (LoadTextureFromFile(path.c_str(), &tex, &w, &h))
        g_skin_textures.push_back({tex, w, h, path});
}

static void CleanupSkinTextures() {
    for (auto& st : g_skin_textures) if (st.tex) glDeleteTextures(1, &st.tex);
    g_skin_textures.clear();
}

static void DrawDirtBG(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
    if (g_dirt_texture != 0) {
        float tile = 32.0f;
        int cols = (int)(size.x / tile) + 2;
        int rows = (int)(size.y / tile) + 2;
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++) {
                float x = pos.x + c * tile, y = pos.y + r * tile;
                dl->AddImage((void*)(intptr_t)g_dirt_texture,
                    ImVec2(x, y), ImVec2(x+tile, y+tile), ImVec2(0,0), ImVec2(1,1));
            }
    } else {
        int cols = (int)(size.x/64)+2, rows = (int)(size.y/64)+2;
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++) {
                float x = pos.x+c*64.f, y = pos.y+r*64.f;
                dl->AddRectFilled(ImVec2(x,y), ImVec2(x+64,y+64),
                    ((r+c)%2==0) ? ToU32(MCColor::Dirt()) : ToU32(MCColor::DirtDark()));
            }
    }
    dl->AddRectFilled(pos, ImVec2(pos.x+size.x, pos.y+size.y), IM_COL32(0,0,0,100));
}

static void ShadowText(ImDrawList* dl, ImVec2 pos, ImU32 color, const char* text) {
    dl->AddText(ImVec2(pos.x+2, pos.y+2), IM_COL32(0,0,0,200), text);
    dl->AddText(pos, color, text);
}

static void DrawMCButton3D(ImVec2 pos, ImVec2 size, bool hovered, bool active) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mx(pos.x+size.x, pos.y+size.y);
    if (active) {
        dl->AddRectFilled(pos, mx, ToU32(MCColor::StoneDark()));
        dl->AddLine(pos, ImVec2(mx.x,pos.y), ToU32(MCColor::Shadow()), 2);
        dl->AddLine(pos, ImVec2(pos.x,mx.y), ToU32(MCColor::Shadow()), 2);
        dl->AddLine(ImVec2(pos.x,mx.y), mx, ToU32(MCColor::StoneLight()), 2);
        dl->AddLine(ImVec2(mx.x,pos.y), mx, ToU32(MCColor::StoneLight()), 2);
    } else {
        dl->AddRectFilled(pos, mx, hovered ? ToU32(MCColor::StoneLight()) : ToU32(MCColor::Stone()));
        dl->AddLine(pos, ImVec2(mx.x,pos.y), ToU32(MCColor::Gray()), 2);
        dl->AddLine(pos, ImVec2(pos.x,mx.y), ToU32(MCColor::Gray()), 2);
        dl->AddLine(ImVec2(pos.x,mx.y), mx, ToU32(MCColor::Shadow()), 2);
        dl->AddLine(ImVec2(mx.x,pos.y), mx, ToU32(MCColor::Shadow()), 2);
    }
    dl->AddRect(pos, mx, ToU32(MCColor::Black()), 0, 0, 1);
}

static bool MCButton(const char* label, ImVec2 size, ImFont* font = nullptr) {
    if (font) ImGui::PushFont(font);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    if (size.x <= 0) size.x = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton(label, size);
    bool clicked = ImGui::IsItemClicked(), hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
    DrawMCButton3D(pos, size, hov, act);
    ImVec2 ts = ImGui::CalcTextSize(label);
    float yo = act ? 2.f : 0.f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(ImVec2(pos.x+(size.x-ts.x)*.5f+2, pos.y+(size.y-ts.y)*.5f+2+yo), IM_COL32(0,0,0,200), label);
    dl->AddText(ImVec2(pos.x+(size.x-ts.x)*.5f, pos.y+(size.y-ts.y)*.5f+yo),
        hov ? ToU32(MCColor::Yellow()) : ToU32(MCColor::White()), label);
    if (font) ImGui::PopFont();
    return clicked;
}

static bool MCButtonColored(const char* label, ImVec2 size, ImVec4 col, ImVec4 hov_col,
                            ImVec4 act_col, ImFont* font = nullptr) {
    if (font) ImGui::PushFont(font);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    if (size.x <= 0) size.x = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton(label, size);
    bool clicked = ImGui::IsItemClicked(), hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mx(pos.x+size.x, pos.y+size.y);
    ImVec4 bg = act ? act_col : (hov ? hov_col : col);
    dl->AddRectFilled(pos, mx, ToU32(bg));
    ImVec4 li(fminf(bg.x+.15f,1), fminf(bg.y+.15f,1), fminf(bg.z+.15f,1), 1);
    ImVec4 dk(bg.x*.5f, bg.y*.5f, bg.z*.5f, 1);
    if (act) {
        dl->AddLine(pos, ImVec2(mx.x,pos.y), ToU32(dk), 2);
        dl->AddLine(pos, ImVec2(pos.x,mx.y), ToU32(dk), 2);
        dl->AddLine(ImVec2(pos.x,mx.y), mx, ToU32(li), 2);
        dl->AddLine(ImVec2(mx.x,pos.y), mx, ToU32(li), 2);
    } else {
        dl->AddLine(pos, ImVec2(mx.x,pos.y), ToU32(li), 2);
        dl->AddLine(pos, ImVec2(pos.x,mx.y), ToU32(li), 2);
        dl->AddLine(ImVec2(pos.x,mx.y), mx, ToU32(dk), 2);
        dl->AddLine(ImVec2(mx.x,pos.y), mx, ToU32(dk), 2);
    }
    dl->AddRect(pos, mx, ToU32(MCColor::Black()), 0, 0, 1);
    ImVec2 ts = ImGui::CalcTextSize(label);
    float yo = act ? 2.f : 0.f;
    ImVec2 tp(pos.x+(size.x-ts.x)*.5f, pos.y+(size.y-ts.y)*.5f+yo);
    dl->AddText(ImVec2(tp.x+2,tp.y+2), IM_COL32(0,0,0,200), label);
    dl->AddText(tp, ToU32(MCColor::White()), label);
    if (font) ImGui::PopFont();
    return clicked;
}

static void DrawProgressBar(ImVec2 pos, ImVec2 size, float frac) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mx(pos.x+size.x, pos.y+size.y);
    dl->AddRectFilled(pos, mx, ToU32(MCColor::Black()));
    dl->AddRectFilled(ImVec2(pos.x+2,pos.y+2), ImVec2(pos.x+2+(size.x-4)*frac, mx.y-2), ToU32(MCColor::Green()));
    dl->AddRectFilled(ImVec2(pos.x+2,pos.y+2), ImVec2(pos.x+2+(size.x-4)*frac, pos.y+size.y*.45f),
        ToU32(ImVec4(0.45f,0.80f,0.30f,1)));
    dl->AddRect(pos, mx, ToU32(MCColor::Shadow()), 0, 0, 2);
}

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(960, 540, "PrismarineLCE", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ApplyStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    g_font_sm = io.Fonts->AddFontFromFileTTF("assets/Minecraft.ttf", 16.0f);
    g_font_md = io.Fonts->AddFontFromFileTTF("assets/Minecraft.ttf", 20.0f);
    g_font_lg = io.Fonts->AddFontFromFileTTF("assets/Minecraft.ttf", 28.0f);
    g_font_xl = io.Fonts->AddFontFromFileTTF("assets/Minecraft.ttf", 48.0f);
    if (!g_font_sm) g_font_sm = io.Fonts->AddFontDefault();
    if (!g_font_md) g_font_md = g_font_sm;
    if (!g_font_lg) g_font_lg = g_font_sm;
    if (!g_font_xl) g_font_xl = g_font_sm;

    LoadTextureFromFile("assets/dirt.png", &g_dirt_texture, &g_dirt_w, &g_dirt_h);

    Launcher launcher;
    launcher.FetchReleases();
    launcher.LoadConfig();
    launcher.ScanSkins();

    bool show_options = false, show_profile = false, show_skins = false, show_servers = false;
    int editing_server_idx = -1;
    bool show_server_edit = false;
    char edit_srv_name[64] = "";
    char edit_srv_ip[128] = "";
    char edit_srv_port[16] = "25565";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int win_w, win_h;
        glfwGetWindowSize(window, &win_w, &win_h);
        float w = (float)win_w, h = (float)win_h;

        ImGui::SetNextWindowPos(ImVec2(0,0));
        ImGui::SetNextWindowSize(ImVec2(w,h));
        ImGui::Begin("##root", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);

        auto& releases = launcher.GetReleases();
        LaunchConfig& cfg = launcher.GetConfig();
        LauncherState st = launcher.GetState();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        DrawDirtBG(dl, ImVec2(0,0), ImVec2(w,h));

        float btn_w = 400.f;
        if (btn_w > w * 0.85f) btn_w = w * 0.85f;
        float cx = (w - btn_w) * 0.5f;

        bool game_exists = launcher.HasGameFiles();
        bool is_current = !releases.empty() && launcher.IsInstalled(cfg.selected_version);
        bool show_update_btn = game_exists && !is_current && !releases.empty() && st == LauncherState::Idle;
        bool show_progress = (st == LauncherState::Downloading || st == LauncherState::Extracting);
        bool disabled = (st == LauncherState::Downloading || st == LauncherState::Extracting ||
                         st == LauncherState::GameRunning || st == LauncherState::FetchingReleases);

        // --- layout: title + subtitle + username + version + play/install + update + progress
        //     + 3 buttons row + 2 buttons row
        float layout_h = 48 + 8 + 16 + 4 + 16 + 20 + 40 + 12 + 54 + 8 + 44 + 8 + 38;
        if (show_update_btn) layout_h += 32 + 8;
        if (show_progress) layout_h += 42 + 8;

        float y = (h - layout_h) * 0.5f;
        if (y < 10) y = 10;

        // Title
        ImGui::PushFont(g_font_xl);
        {
            const char* t = "PrismarineLCE";
            float tw = ImGui::CalcTextSize(t).x;
            ShadowText(dl, ImVec2((w-tw)*.5f, y), ToU32(MCColor::White()), t);
        }
        ImGui::PopFont();
        y += 48 + 8;

        // Subtitle
        ImGui::PushFont(g_font_sm);
        {
            const char* s = "Minecraft Legacy Console Edition";
            float sw2 = ImGui::CalcTextSize(s).x;
            ShadowText(dl, ImVec2((w-sw2)*.5f, y), ToU32(MCColor::Gray()), s);
        }
        ImGui::PopFont();
        y += 16 + 4;

        // Username
        ImGui::PushFont(g_font_sm);
        {
            char ut[128];
            snprintf(ut, sizeof(ut), "Playing as: %s", cfg.username);
            float utw = ImGui::CalcTextSize(ut).x;
            ShadowText(dl, ImVec2((w-utw)*.5f, y), ToU32(MCColor::Gray()), ut);
        }
        ImGui::PopFont();
        y += 16 + 20;

        // Version combo
        if (!releases.empty()) {
            ImGui::SetCursorPos(ImVec2(cx, y));
            ImGui::PushFont(g_font_md);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14,12));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8,10));
            ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 2.f);
            ImGui::PushItemWidth(btn_w);

            std::string sel_display = releases[cfg.selected_version].name;
            if (releases[cfg.selected_version].published_at.size() >= 10)
                sel_display += "  (" + releases[cfg.selected_version].published_at.substr(0,10) + ")";

            if (ImGui::BeginCombo("##ver", sel_display.c_str())) {
                for (int i = 0; i < (int)releases.size(); i++) {
                    bool sel = (cfg.selected_version == i);
                    std::string lbl = releases[i].name;
                    if (releases[i].published_at.size() >= 10)
                        lbl += "  (" + releases[i].published_at.substr(0,10) + ")";
                    if (ImGui::Selectable(lbl.c_str(), sel))
                        cfg.selected_version = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::PopStyleVar(3);
            ImGui::PopFont();
        } else {
            ImGui::SetCursorPos(ImVec2(cx, y));
            ImGui::PushFont(g_font_md);
            ImGui::PushStyleColor(ImGuiCol_Text, MCColor::Gray());
            ImGui::Text("%s", launcher.GetStatusText());
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }
        y += 40 + 12;

        // Play / Install button
        ImGui::SetCursorPos(ImVec2(cx, y));
        if (game_exists) {
            const char* play_label = "Play";
            ImVec4 pcol = MCColor::Green(), phov = ImVec4(.4f,.75f,.26f,1), pact = MCColor::GreenDark();
            if (st == LauncherState::GameRunning) {
                play_label = "Game Running...";
                pcol = phov = pact = MCColor::StoneDark();
            } else if (st == LauncherState::Downloading || st == LauncherState::Extracting) {
                play_label = "Working...";
                pcol = phov = pact = MCColor::StoneDark();
            }
            if (disabled) ImGui::BeginDisabled();
            if (MCButtonColored(play_label, ImVec2(btn_w, 54), pcol, phov, pact, g_font_lg))
                launcher.Launch();
            if (disabled) ImGui::EndDisabled();
        } else {
            const char* inst_label = "Install";
            ImVec4 pcol = MCColor::Green(), phov = ImVec4(.4f,.75f,.26f,1), pact = MCColor::GreenDark();
            if (st == LauncherState::Downloading || st == LauncherState::Extracting) {
                inst_label = "Working...";
                pcol = phov = pact = MCColor::StoneDark();
            }
            if (disabled) ImGui::BeginDisabled();
            if (MCButtonColored(inst_label, ImVec2(btn_w, 54), pcol, phov, pact, g_font_lg)) {
                if (!releases.empty()) launcher.DownloadAndInstall(cfg.selected_version);
            }
            if (disabled) ImGui::EndDisabled();
        }
        y += 54 + 8;

        // Update button
        if (show_update_btn) {
            ImGui::SetCursorPos(ImVec2(cx, y));
            if (MCButton("Update Available - Click to Update", ImVec2(btn_w, 32), g_font_sm))
                launcher.DownloadAndInstall(cfg.selected_version);
            y += 32 + 8;
        }

        // Progress bar
        if (show_progress) {
            ImGui::SetCursorPos(ImVec2(cx, y));
            DrawProgressBar(ImGui::GetCursorScreenPos(), ImVec2(btn_w, 16), launcher.GetProgress());
            ImGui::Dummy(ImVec2(btn_w, 16));
            ImGui::PushFont(g_font_sm);
            const char* stxt = launcher.GetStatusText();
            float stw = ImGui::CalcTextSize(stxt).x;
            ImGui::SetCursorPos(ImVec2((w-stw)*.5f, y + 22));
            ImGui::PushStyleColor(ImGuiCol_Text, MCColor::Gray());
            ImGui::Text("%s", stxt);
            ImGui::PopStyleColor();
            ImGui::PopFont();
            y += 42 + 8;
        }

        // Row of 3: Profile, Options, Skins
        float third = (btn_w - 12) / 3.f;
        ImGui::SetCursorPos(ImVec2(cx, y));
        if (MCButton("Profile", ImVec2(third, 40), g_font_md)) show_profile = true;
        ImGui::SetCursorPos(ImVec2(cx + third + 4, y));
        if (MCButton("Options", ImVec2(third, 40), g_font_md)) show_options = true;
        ImGui::SetCursorPos(ImVec2(cx + 2 * (third + 4), y));
        if (MCButton("Skins", ImVec2(third, 40), g_font_md)) show_skins = true;
        y += 40 + 8;

        // Row of 2: Servers, Open Game Folder
        float half = (btn_w - 8) / 2.f;
        ImGui::SetCursorPos(ImVec2(cx, y));
        if (MCButton("Servers", ImVec2(half, 34), g_font_sm)) show_servers = true;
        ImGui::SetCursorPos(ImVec2(cx + half + 8, y));
        if (MCButton("Open Game Folder", ImVec2(half, 34), g_font_sm)) {
#ifdef _WIN32
            std::string path = launcher.GetInstallDir().string();
            fs::create_directories(launcher.GetInstallDir());
            ShellExecuteA(NULL, "explore", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
            fs::create_directories(launcher.GetInstallDir());
            std::string cmd = "xdg-open \"" + launcher.GetInstallDir().string() + "\"";
            std::system(cmd.c_str());
#endif
        }
        y += 34 + 4;

        // Bottom path
        {
            ImGui::PushFont(g_font_sm);
            std::string dir_str = launcher.GetInstallDir().string();
            float dw2 = ImGui::CalcTextSize(dir_str.c_str()).x;
            float fdy = h - 28, fdx = (w-dw2)*.5f;
            ImVec2 scr = ImGui::GetWindowPos();
            float fh = ImGui::GetFontSize();
            dl->AddRectFilled(ImVec2(scr.x+fdx-8, scr.y+fdy-3),
                ImVec2(scr.x+fdx+dw2+8, scr.y+fdy+fh+3), IM_COL32(0,0,0,160));
            ShadowText(dl, ImVec2(scr.x+fdx, scr.y+fdy), ToU32(MCColor::Gray()), dir_str.c_str());
            ImGui::PopFont();
        }


        // ============ OPTIONS MODAL ============
        if (show_options) { ImGui::OpenPopup("##opts"); show_options = false; }

        ImVec2 msz(500, 420);
        ImGui::SetNextWindowSize(msz, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2((w-msz.x)*.5f, (h-msz.y)*.5f), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(30,24));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, MCColor::BgPanel());
        ImGui::PushStyleColor(ImGuiCol_Border, MCColor::Black());

        if (ImGui::BeginPopupModal("##opts", nullptr,
            ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoTitleBar)) {

            // Title
            ImGui::PushFont(g_font_lg);
            ImVec2 cp = ImGui::GetCursorScreenPos();
            dl->AddText(ImVec2(cp.x+2,cp.y+2), IM_COL32(0,0,0,200), "Options");
            ImGui::Text("Options");
            ImGui::PopFont();
            ImGui::Spacing(); ImGui::Spacing();

            ImGui::PushFont(g_font_md);
            ImGui::PushItemWidth(-1);

            // Server dropdown or placeholder
            ImGui::PushStyleColor(ImGuiCol_Text, MCColor::Gray());
            ImGui::Text("Default Server");
            ImGui::PopStyleColor();

            if (!cfg.server_list.empty()) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10,10));
                ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 2.f);
                ImGui::PushStyleColor(ImGuiCol_Text, MCColor::White());
                if (ImGui::BeginCombo("##server", cfg.ip)) {
                    for (int i = 0; i < (int)cfg.server_list.size(); i++) {
                        bool sel = (std::string(cfg.ip) == std::string(cfg.server_list[i].ip) &&
                                    std::string(cfg.port) == std::string(cfg.server_list[i].port));
                        if (ImGui::Selectable(cfg.server_list[i].name, sel)) {
                            std::strncpy(cfg.ip, cfg.server_list[i].ip, sizeof(cfg.ip) - 1);
                            cfg.ip[sizeof(cfg.ip) - 1] = '\0';
                            std::strncpy(cfg.port, cfg.server_list[i].port, sizeof(cfg.port) - 1);
                            cfg.port[sizeof(cfg.port) - 1] = '\0';
                        }
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopStyleColor(1);
                ImGui::PopStyleVar(2);
            } else {
                ImGui::SetNextItemWidth(-1);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10,10));
                ImGui::PushStyleColor(ImGuiCol_Text, MCColor::DarkGray());
                ImGui::PushStyleColor(ImGuiCol_FrameBg, MCColor::DarkGray());
                ImGui::InputText("##servernone", (char*)"Add a server below", 0, ImGuiInputTextFlags_ReadOnly);
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar(1);
            }
            ImGui::Spacing();

            // Manual IP / Port
            ImGui::PushStyleColor(ImGuiCol_Text, MCColor::Gray());
            ImGui::Text("Or manually enter:");
            ImGui::PopStyleColor();
            ImGui::Spacing();

            float opt_half = (ImGui::GetContentRegionAvail().x - 8) * .5f;
            ImGui::SetNextItemWidth(opt_half);
            ImGui::InputText("##ip", cfg.ip, sizeof(cfg.ip));
            ImGui::SameLine(0, 8);
            ImGui::SetNextItemWidth(opt_half);
            ImGui::InputText("##port", cfg.port, sizeof(cfg.port));
            ImGui::Spacing();

            ImGui::Checkbox("Fullscreen", &cfg.fullscreen);
            ImGui::Checkbox("Headless Server (-server)", &cfg.is_server);
#ifndef _WIN32
            ImGui::Checkbox("Use Wine", &cfg.use_wine);
#endif

            ImGui::PopItemWidth();
            ImGui::PopFont();
            ImGui::Spacing(); ImGui::Spacing();

            float btn_h2 = 34;
            float btn_w2 = (msz.x - 76) * .5f;
            if (MCButton("Manage Servers", ImVec2(btn_w2, btn_h2), g_font_sm)) show_servers = true;
            ImGui::SameLine(0, 8);
            if (MCButtonColored("Done", ImVec2(btn_w2, btn_h2),
                MCColor::Green(), ImVec4(.4f,.75f,.26f,1), MCColor::GreenDark(), g_font_sm)) {
                launcher.SaveConfig();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);


        // ============ PROFILE MODAL ============
        if (show_profile) { ImGui::OpenPopup("##prof"); show_profile = false; }
        ImVec2 psz(500, 280);
        ImGui::SetNextWindowSize(psz, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2((w-psz.x)*.5f, (h-psz.y)*.5f), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(30,24));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, MCColor::BgPanel());
        ImGui::PushStyleColor(ImGuiCol_Border, MCColor::Black());

        if (ImGui::BeginPopupModal("##prof", nullptr,
            ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoTitleBar)) {

            ImGui::PushFont(g_font_lg);
            ImVec2 cp = ImGui::GetCursorScreenPos();
            dl->AddText(ImVec2(cp.x+2,cp.y+2), IM_COL32(0,0,0,200), "Profile");
            ImGui::Text("Profile");
            ImGui::PopFont();
            ImGui::Spacing(); ImGui::Spacing();

            ImGui::PushFont(g_font_md);
            ImGui::PushItemWidth(-1);
            ImGui::PushStyleColor(ImGuiCol_Text, MCColor::Gray());
            ImGui::Text("Username");
            ImGui::PopStyleColor();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10,10));
            ImGui::InputText("##user", cfg.username, sizeof(cfg.username));
            ImGui::PopStyleVar();
            ImGui::PopItemWidth();
            ImGui::PopFont();
            ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

            if (MCButtonColored("Save", ImVec2(-1, 54),
                MCColor::Green(), ImVec4(.4f,.75f,.26f,1), MCColor::GreenDark(), g_font_lg)) {
                launcher.SaveConfig();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);


        // ============ SKINS MODAL ============
        if (show_skins) { ImGui::OpenPopup("##skins"); show_skins = false; }
        ImVec2 sksz(600, 460);
        ImGui::SetNextWindowSize(sksz, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2((w-sksz.x)*.5f, (h-sksz.y)*.5f), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20,20));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, MCColor::BgPanel());
        ImGui::PushStyleColor(ImGuiCol_Border, MCColor::Black());

        if (ImGui::BeginPopupModal("##skins", nullptr,
            ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoTitleBar)) {

            ImGui::PushFont(g_font_lg);
            dl->AddText(ImVec2(ImGui::GetCursorScreenPos().x+2,ImGui::GetCursorScreenPos().y+2), IM_COL32(0,0,0,200), "Select Skin");
            ImGui::Text("Select Skin");
            ImGui::PopFont();
            ImGui::Separator();

            if (cfg.skins.empty()) {
                ImGui::PushFont(g_font_md);
                ImGui::PushStyleColor(ImGuiCol_Text, MCColor::DarkGray());
                ImGui::Text("No skins found in game directory.");
                ImGui::Text("Skins are loaded from Windows64/ folder.");
                ImGui::PopStyleColor();
                ImGui::PopFont();
            } else {
                float skin_w = 128, skin_h = 128;
                int skin_cols = (int)((sksz.x - 60) / (skin_w + 12));
                if (skin_cols < 1) skin_cols = 1;

                ImGui::BeginChild("##skin_list", ImVec2(sksz.x - 40, sksz.y - 120), true);
                for (int i = 0; i < (int)cfg.skins.size(); i++) {
                    EnsureSkinTextureLoaded(cfg.skins[i].path);
                    int col = i % skin_cols;
                    int row = i / skin_cols;
                    ImVec2 spos = ImVec2(10 + col * (skin_w + 12), row * (skin_h + 16 + 30));
                    ImGui::SetCursorPos(spos);

                    bool sel = (i == cfg.selected_skin);
                    std::string btn_id = "##skin_sel_" + std::to_string(i);
                    ImGui::InvisibleButton(btn_id.c_str(), ImVec2(skin_w, skin_h + 16 + 20));
                    bool clicked = ImGui::IsItemClicked();
                    bool hov = ImGui::IsItemHovered();

                    ImVec2 sp = ImGui::GetItemRectMin();
                    ImVec2 sm = ImGui::GetItemRectMax();

                    ImU32 borderColor = sel ? ToU32(MCColor::Green()) : (hov ? ToU32(MCColor::StoneLight()) : ToU32(MCColor::Stone()));

                    GLuint tex = 0;
                    for (auto& st : g_skin_textures) {
                        if (st.path == cfg.skins[i].path) { tex = st.tex; break; }
                    }

                    if (tex) {
                        ImVec2 img_pos(sp.x + 4, sp.y + 4);
                        ImVec2 img_size(skin_w - 8, skin_h - 12);
                        dl->AddRectFilled(sp, sm, ToU32(MCColor::StoneDark()), 2);
                        dl->AddImage((void*)(intptr_t)tex, img_pos,
                            ImVec2(img_pos.x + img_size.x, img_pos.y + img_size.x),
                            ImVec2(0,0), ImVec2(1,0.25f));
                    } else {
                        dl->AddRectFilled(sp, sm, ToU32(MCColor::StoneDark()));
                    }

                    dl->AddRect(sp, sm, borderColor, 2, 0, 3);
                    if (sel) dl->AddRect(sp, sm, ToU32(MCColor::Green()), 2, 0, 2);

                    ImVec2 name_pos(sp.x, sp.y + skin_h + 4);
                    std::string sname = cfg.skins[i].name;
                    if (sname.size() > 16) sname = sname.substr(0, 16);
                    dl->AddText(name_pos, sel ? ToU32(MCColor::Green()) : ToU32(MCColor::Gray()), sname.c_str());

                    if (clicked) cfg.selected_skin = i;
                }
                ImGui::EndChild();
            }
            ImGui::Separator();

            if (MCButtonColored("Save & Close", ImVec2(sksz.x - 40, 54),
                MCColor::Green(), ImVec4(.4f,.75f,.26f,1), MCColor::GreenDark(), g_font_lg)) {
                launcher.SaveConfig();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);


        // ============ SERVER LIST MODAL ============
        if (show_servers) { ImGui::OpenPopup("##srvlist"); show_servers = false; }
        ImVec2 srvsz(500, 400);
        ImGui::SetNextWindowSize(srvsz, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2((w-srvsz.x)*.5f, (h-srvsz.y)*.5f), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20,20));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, MCColor::BgPanel());
        ImGui::PushStyleColor(ImGuiCol_Border, MCColor::Black());

        if (ImGui::BeginPopupModal("##srvlist", nullptr,
            ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoTitleBar)) {

            ImGui::PushFont(g_font_lg);
            dl->AddText(ImVec2(ImGui::GetCursorScreenPos().x+2,ImGui::GetCursorScreenPos().y+2), IM_COL32(0,0,0,200), "Server List");
            ImGui::Text("Server List");
            ImGui::PopFont();
            ImGui::Separator();

            ImGui::BeginChild("##srv_list", ImVec2(srvsz.x - 40, srvsz.y - 180), true);
            ImGui::PushFont(g_font_sm);
            for (int i = 0; i < (int)cfg.server_list.size(); i++) {
                std::string item = std::string(cfg.server_list[i].name) + " (" +
                                   cfg.server_list[i].ip + ":" + cfg.server_list[i].port + ")";
                ImGui::PushID(i);
                float row_w = srvsz.x - 80;
                float small_w = 30;
                float btn_w3 = (row_w - 8 - 8) - (small_w + 4 + small_w + 4);
                if (MCButton(item.c_str(), ImVec2(btn_w3, 32), g_font_sm)) {
                    std::strncpy(cfg.ip, cfg.server_list[i].ip, sizeof(cfg.ip) - 1);
                    cfg.ip[sizeof(cfg.ip) - 1] = '\0';
                    std::strncpy(cfg.port, cfg.server_list[i].port, sizeof(cfg.port) - 1);
                    cfg.port[sizeof(cfg.port) - 1] = '\0';
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine(0, 4);
                if (MCButton("Edit", ImVec2(small_w, 32))) {
                    editing_server_idx = i;
                    std::strncpy(edit_srv_name, cfg.server_list[i].name, sizeof(edit_srv_name) - 1);
                    edit_srv_name[sizeof(edit_srv_name) - 1] = '\0';
                    std::strncpy(edit_srv_ip, cfg.server_list[i].ip, sizeof(edit_srv_ip) - 1);
                    edit_srv_ip[sizeof(edit_srv_ip) - 1] = '\0';
                    std::strncpy(edit_srv_port, cfg.server_list[i].port, sizeof(edit_srv_port) - 1);
                    edit_srv_port[sizeof(edit_srv_port) - 1] = '\0';
                    show_server_edit = true;
                }
                ImGui::SameLine(0, 4);
                if (MCButtonColored("X", ImVec2(small_w, 32), MCColor::Dirt(), ImVec4(.4f,.25f,.15f,1), MCColor::DirtDark())) {
                    launcher.RemoveServer(i);
                }
                ImGui::PopID();
            }
            if (cfg.server_list.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, MCColor::DarkGray());
                ImGui::Text("No servers added yet.");
                ImGui::PopStyleColor();
            }
            ImGui::PopFont();
            ImGui::EndChild();

            ImGui::Spacing();
            if (MCButton("Add Server", ImVec2(srvsz.x - 40, 32), g_font_sm)) {
                editing_server_idx = -1;
                edit_srv_name[0] = '\0';
                edit_srv_ip[0] = '\0';
                std::strncpy(edit_srv_port, "25565", sizeof(edit_srv_port) - 1);
                show_server_edit = true;
            }
            ImGui::Separator();

            if (MCButtonColored("Done", ImVec2(srvsz.x - 40, 32),
                MCColor::Green(), ImVec4(.4f,.75f,.26f,1), MCColor::GreenDark(), g_font_sm)) {
                launcher.SaveConfig();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);


        // ============ SERVER EDIT MODAL ============
        if (show_server_edit) { ImGui::OpenPopup("##srvedit"); show_server_edit = false; }
        ImVec2 edtsz(400, 300);
        ImGui::SetNextWindowSize(edtsz, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2((w-edtsz.x)*.5f, (h-edtsz.y)*.5f), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20,20));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, MCColor::BgPanel());
        ImGui::PushStyleColor(ImGuiCol_Border, MCColor::Black());

        if (ImGui::BeginPopupModal("##srvedit", nullptr,
            ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoTitleBar)) {

            ImGui::PushFont(g_font_lg);
            ImGui::Text(editing_server_idx >= 0 ? "Edit Server" : "Add Server");
            ImGui::PopFont();
            ImGui::Separator();

            ImGui::PushFont(g_font_sm);
            ImGui::PushItemWidth(-1);
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, MCColor::Gray());
            ImGui::Text("Name");
            ImGui::PopStyleColor();
            ImGui::InputText("##srvname", edit_srv_name, sizeof(edit_srv_name));

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, MCColor::Gray());
            ImGui::Text("IP");
            ImGui::PopStyleColor();
            ImGui::InputText("##srvip", edit_srv_ip, sizeof(edit_srv_ip));

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, MCColor::Gray());
            ImGui::Text("Port");
            ImGui::PopStyleColor();
            ImGui::InputText("##srvport", edit_srv_port, sizeof(edit_srv_port));

            ImGui::PopItemWidth();
            ImGui::PopFont();
            ImGui::Spacing();

            if (MCButtonColored("Save", ImVec2(edtsz.x - 40, 32),
                MCColor::Green(), ImVec4(.4f,.75f,.26f,1), MCColor::GreenDark(), g_font_sm)) {
                ServerEntry entry;
                std::strncpy(entry.name, edit_srv_name, sizeof(entry.name) - 1);
                entry.name[sizeof(entry.name) - 1] = '\0';
                std::strncpy(entry.ip, edit_srv_ip, sizeof(entry.ip) - 1);
                entry.ip[sizeof(entry.ip) - 1] = '\0';
                std::strncpy(entry.port, edit_srv_port, sizeof(entry.port) - 1);
                entry.port[sizeof(entry.port) - 1] = '\0';
                if (editing_server_idx >= 0)
                    launcher.UpdateServer(editing_server_idx, entry);
                else
                    launcher.AddServer(entry);
                launcher.SaveConfig();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (MCButton("Cancel", ImVec2(-1, 32), g_font_sm)) ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        ImGui::End();

        ImGui::Render();
        int dw2, dh2;
        glfwGetFramebufferSize(window, &dw2, &dh2);
        glViewport(0, 0, dw2, dh2);
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    if (g_dirt_texture) glDeleteTextures(1, &g_dirt_texture);
    CleanupSkinTextures();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}