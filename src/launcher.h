#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <filesystem>

namespace fs = std::filesystem;

enum class LauncherState {
    Idle,
    FetchingReleases,
    Downloading,
    Extracting,
    GameRunning,
    Error
};

struct ReleaseInfo {
    std::string tag;
    std::string name;
    std::string published_at;
    std::string zip_url;
    std::string exe_url;
    bool has_zip = false;
    bool has_exe = false;
    std::string GetUniqueId() const { return tag + "|" + published_at; }
};

struct LaunchConfig {
    char username[64] = "Player";
    char ip[128] = "";
    char port[16] = "25565";
    bool is_server = false;
    bool use_wine = false;
    int selected_version = 0;
};

class Launcher {
public:
    Launcher();
    ~Launcher();

    void FetchReleases();
    void DownloadAndInstall(int release_index);
    void UpdateExeOnly(int release_index);
    void Launch();
    void LoadConfig();
    void SaveConfig();

    LauncherState GetState() const;
    float GetProgress() const;
    const char* GetStatusText() const;
    const std::vector<ReleaseInfo>& GetReleases() const;
    LaunchConfig& GetConfig();

    bool IsInstalled(int release_index) const;
    bool HasGameFiles() const;
    fs::path GetInstallDir() const;

private:
    static constexpr const char* REPO_OWNER = "smartcmd";
    static constexpr const char* REPO_NAME  = "MinecraftConsoles";
    static constexpr const char* TARGET_ZIP = "LCEWindows64.zip";
    static constexpr const char* TARGET_EXE = "Minecraft.Client.exe";

    fs::path GetZipPath() const;
    fs::path GetExePath() const;
    std::string GetInstalledId() const;
    void SaveInstalledId(const std::string& id);
    void SetStatus(const char* text);

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static int ProgressCallback(void* clientp, double dltotal, double dlnow, double, double);
    std::string HttpGet(const std::string& url);
    bool DownloadFile(const std::string& url, const fs::path& dest);
    bool ExtractZip(const fs::path& zip_path, const fs::path& dest_dir);

    std::atomic<LauncherState> state{LauncherState::Idle};
    std::atomic<float> progress{0.0f};
    char status_text[256] = "Ready";
    std::mutex mutex;
    std::thread worker;
    std::vector<ReleaseInfo> releases;
    LaunchConfig config;
};