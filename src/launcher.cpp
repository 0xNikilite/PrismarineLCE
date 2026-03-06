#include "launcher.h"

#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

static std::string JsonExtractString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    size_t end = pos + 1;
    while (end < json.size() && json[end] != '"') {
        if (json[end] == '\\') end++;
        end++;
    }
    return json.substr(pos + 1, end - pos - 1);
}

static std::vector<std::string> JsonSplitArray(const std::string& json) {
    std::vector<std::string> items;
    int depth = 0;
    size_t start = 0;
    bool in_string = false;
    for (size_t i = 0; i < json.size(); i++) {
        char c = json[i];
        if (c == '"' && (i == 0 || json[i-1] != '\\')) {
            in_string = !in_string;
            continue;
        }
        if (in_string) continue;
        if (c == '{' || c == '[') {
            if (depth == 0) start = i;
            depth++;
        } else if (c == '}' || c == ']') {
            depth--;
            if (depth == 0) items.push_back(json.substr(start, i - start + 1));
        }
    }
    return items;
}

static std::string JsonGetArray(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find('[', pos);
    if (pos == std::string::npos) return "";
    int depth = 0;
    size_t end = pos;
    bool in_string = false;
    for (; end < json.size(); end++) {
        char c = json[end];
        if (c == '"' && (end == 0 || json[end-1] != '\\')) {
            in_string = !in_string;
            continue;
        }
        if (in_string) continue;
        if (c == '[') depth++;
        else if (c == ']') {
            depth--;
            if (depth == 0) break;
        }
    }
    return json.substr(pos, end - pos + 1);
}

Launcher::Launcher() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

Launcher::~Launcher() {
    if (worker.joinable()) worker.join();
    curl_global_cleanup();
}

fs::path Launcher::GetInstallDir() const {
#ifdef _WIN32
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK)
        return fs::path(appdata) / ".prismarine";
    const char* env = std::getenv("APPDATA");
    if (env) return fs::path(env) / ".prismarine";
    return fs::current_path() / ".prismarine";
#else
    const char* home = std::getenv("HOME");
    if (home) return fs::path(home) / ".prismarine";
    return fs::current_path() / ".prismarine";
#endif
}

fs::path Launcher::GetZipPath() const {
    return GetInstallDir() / TARGET_ZIP;
}

fs::path Launcher::GetExePath() const {
    return GetInstallDir() / TARGET_EXE;
}

std::string Launcher::GetInstalledId() const {
    fs::path id_file = GetInstallDir() / ".installed_version";
    if (!fs::exists(id_file)) return "";
    std::ifstream f(id_file);
    std::string id;
    std::getline(f, id);
    return id;
}

void Launcher::SaveInstalledId(const std::string& id) {
    fs::create_directories(GetInstallDir());
    fs::path id_file = GetInstallDir() / ".installed_version";
    std::ofstream f(id_file);
    f << id;
}

LauncherState Launcher::GetState() const { return state.load(); }
float Launcher::GetProgress() const { return progress.load(); }
const char* Launcher::GetStatusText() const { return status_text; }
const std::vector<ReleaseInfo>& Launcher::GetReleases() const { return releases; }
LaunchConfig& Launcher::GetConfig() { return config; }

bool Launcher::IsInstalled(int idx) const {
    if (idx < 0 || idx >= (int)releases.size()) return false;
    return fs::exists(GetExePath()) && GetInstalledId() == releases[idx].GetUniqueId();
}

bool Launcher::HasGameFiles() const {
    return fs::exists(GetExePath());
}

void Launcher::SetStatus(const char* text) {
    std::lock_guard<std::mutex> lock(mutex);
    std::strncpy(status_text, text, sizeof(status_text) - 1);
    status_text[sizeof(status_text) - 1] = '\0';
}

size_t Launcher::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

int Launcher::ProgressCallback(void* clientp, double dltotal, double dlnow, double, double) {
    auto* launcher = static_cast<Launcher*>(clientp);
    if (dltotal > 0)
        launcher->progress = static_cast<float>(dlnow / dltotal);
    return 0;
}

std::string Launcher::HttpGet(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Prismarine/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) return "";
    return response;
}

bool Launcher::DownloadFile(const std::string& url, const fs::path& dest) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    fs::create_directories(dest.parent_path());
    FILE* fp = fopen(dest.string().c_str(), "wb");
    if (!fp) { curl_easy_cleanup(curl); return false; }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Prismarine/1.0");
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

bool Launcher::ExtractZip(const fs::path& zip_path, const fs::path& dest_dir) {
    fs::create_directories(dest_dir);
    std::string cmd;
#ifdef _WIN32
    cmd = "powershell.exe -NoProfile -Command \"Expand-Archive -Force -Path '"
        + zip_path.string() + "' -DestinationPath '" + dest_dir.string() + "'\"";
#else
    cmd = "unzip -o '" + zip_path.string() + "' -d '" + dest_dir.string() + "'";
#endif
    int result = std::system(cmd.c_str());
    if (result == 0) {
        std::error_code ec;
        fs::remove(zip_path, ec);
    }
    return result == 0;
}

void Launcher::FetchReleases() {
    if (state == LauncherState::FetchingReleases) return;
    if (worker.joinable()) worker.join();
    worker = std::thread([this]() {
        state = LauncherState::FetchingReleases;
        SetStatus("Fetching releases...");
        progress = 0.0f;

        std::string url = std::string("https://api.github.com/repos/")
            + REPO_OWNER + "/" + REPO_NAME + "/releases";
        std::string response = HttpGet(url);
        if (response.empty()) {
            SetStatus("Failed to connect to GitHub");
            state = LauncherState::Error;
            return;
        }

        std::vector<ReleaseInfo> parsed;
        auto items = JsonSplitArray(response);
        for (auto& item : items) {
            ReleaseInfo info;
            info.tag = JsonExtractString(item, "tag_name");
            info.name = JsonExtractString(item, "name");
            info.published_at = JsonExtractString(item, "published_at");
            if (info.name.empty()) info.name = info.tag;

            std::string assets_str = JsonGetArray(item, "assets");
            auto assets = JsonSplitArray(assets_str);
            for (auto& asset : assets) {
                std::string asset_name = JsonExtractString(asset, "name");
                std::string dl_url = JsonExtractString(asset, "browser_download_url");
                if (asset_name == TARGET_ZIP) {
                    info.zip_url = dl_url;
                    info.has_zip = true;
                } else if (asset_name == TARGET_EXE) {
                    info.exe_url = dl_url;
                    info.has_exe = true;
                }
            }
            if (!info.tag.empty() && (info.has_zip || info.has_exe))
                parsed.push_back(info);
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            releases = parsed;
        }

        if (releases.empty()) {
            SetStatus("No compatible releases found");
            state = LauncherState::Error;
        } else {
            std::string msg = "Found: " + releases[0].name;
            if (releases[0].published_at.size() >= 10)
                msg += " (" + releases[0].published_at.substr(0, 10) + ")";
            SetStatus(msg.c_str());
            state = LauncherState::Idle;
        }
    });
}

void Launcher::DownloadAndInstall(int idx) {
    if (state != LauncherState::Idle && state != LauncherState::Error) return;
    if (idx < 0 || idx >= (int)releases.size()) return;
    if (worker.joinable()) worker.join();

    worker = std::thread([this, idx]() {
        const ReleaseInfo& rel = releases[idx];

        if (HasGameFiles() && rel.has_exe) {
            state = LauncherState::Downloading;
            progress = 0.0f;
            SetStatus("Updating Minecraft.Client.exe...");
            fs::path exe_dest = GetExePath();
            if (fs::exists(exe_dest)) {
                std::error_code ec;
                fs::remove(exe_dest, ec);
            }
            if (!DownloadFile(rel.exe_url, exe_dest)) {
                SetStatus("Download failed");
                state = LauncherState::Error;
                return;
            }
            SaveInstalledId(rel.GetUniqueId());
            SetStatus("Ready to play!");
            state = LauncherState::Idle;
            return;
        }

        if (!rel.has_zip) {
            SetStatus("No zip download available");
            state = LauncherState::Error;
            return;
        }

        state = LauncherState::Downloading;
        progress = 0.0f;
        SetStatus("Downloading LCEWindows64.zip (~778 MB)...");

        if (!DownloadFile(rel.zip_url, GetZipPath())) {
            SetStatus("Download failed");
            state = LauncherState::Error;
            return;
        }

        state = LauncherState::Extracting;
        progress = 0.0f;
        SetStatus("Extracting game files...");

        if (!ExtractZip(GetZipPath(), GetInstallDir())) {
            SetStatus("Extraction failed");
            state = LauncherState::Error;
            return;
        }

        SaveInstalledId(rel.GetUniqueId());
        SetStatus("Ready to play!");
        state = LauncherState::Idle;
    });
}

void Launcher::UpdateExeOnly(int idx) {
    if (idx < 0 || idx >= (int)releases.size()) return;
    if (!releases[idx].has_exe) return;
    if (state != LauncherState::Idle && state != LauncherState::Error) return;
    if (worker.joinable()) worker.join();

    worker = std::thread([this, idx]() {
        const ReleaseInfo& rel = releases[idx];
        state = LauncherState::Downloading;
        progress = 0.0f;
        SetStatus("Updating Minecraft.Client.exe...");

        fs::path exe_dest = GetExePath();
        if (fs::exists(exe_dest)) {
            std::error_code ec;
            fs::remove(exe_dest, ec);
        }

        if (!DownloadFile(rel.exe_url, exe_dest)) {
            SetStatus("Update failed");
            state = LauncherState::Error;
            return;
        }

        SaveInstalledId(rel.GetUniqueId());
        SetStatus("Ready to play!");
        state = LauncherState::Idle;
    });
}

void Launcher::Launch() {
    if (state == LauncherState::GameRunning) return;
    if (!fs::exists(GetExePath())) {
        SetStatus("Game not installed");
        state = LauncherState::Error;
        return;
    }

#ifndef _WIN32
    try {
        fs::permissions(GetExePath(),
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add);
    } catch (...) {}
#endif

    if (worker.joinable()) worker.join();

    worker = std::thread([this]() {
        state = LauncherState::GameRunning;
        SetStatus("Game running...");

        std::string args;
        if (std::strlen(config.username) > 0)
            args += " -name \"" + std::string(config.username) + "\"";
        if (config.is_server) args += " -server";
        if (std::strlen(config.ip) > 0)
            args += " -ip \"" + std::string(config.ip) + "\"";
        if (std::strlen(config.port) > 0)
            args += " -port \"" + std::string(config.port) + "\"";

#ifdef _WIN32
        std::string exe = GetExePath().string();
        std::string cmdline = "\"" + exe + "\"" + args;
        std::string workdir = GetInstallDir().string();

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {};

        BOOL ok = CreateProcessA(
            exe.c_str(),
            const_cast<char*>(cmdline.c_str()),
            nullptr, nullptr, FALSE, 0, nullptr,
            workdir.c_str(), &si, &pi
        );

        if (ok) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), "Launch failed (error %lu)", GetLastError());
            SetStatus(buf);
            state = LauncherState::Error;
            return;
        }
#else
        std::string cmd;
        std::string exe = GetExePath().string();
        if (config.use_wine)
            cmd = "wine \"" + exe + "\"" + args;
        else
            cmd = "\"" + exe + "\"" + args;
        std::system(cmd.c_str());
#endif

        SetStatus("Ready");
        state = LauncherState::Idle;
    });
}

void Launcher::LoadConfig() {
    fs::path cfg_file = GetInstallDir() / "prismarine.cfg";
    if (!fs::exists(cfg_file)) return;
    std::ifstream f(cfg_file);
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "username") {
            std::strncpy(config.username, val.c_str(), sizeof(config.username) - 1);
            config.username[sizeof(config.username) - 1] = '\0';
        } else if (key == "ip") {
            std::strncpy(config.ip, val.c_str(), sizeof(config.ip) - 1);
            config.ip[sizeof(config.ip) - 1] = '\0';
        } else if (key == "port") {
            std::strncpy(config.port, val.c_str(), sizeof(config.port) - 1);
            config.port[sizeof(config.port) - 1] = '\0';
        } else if (key == "is_server") {
            config.is_server = (val == "1");
        } else if (key == "use_wine") {
            config.use_wine = (val == "1");
        }
    }
}

void Launcher::SaveConfig() {
    fs::create_directories(GetInstallDir());
    fs::path cfg_file = GetInstallDir() / "prismarine.cfg";
    std::ofstream f(cfg_file);
    f << "username=" << config.username << "\n";
    f << "ip=" << config.ip << "\n";
    f << "port=" << config.port << "\n";
    f << "is_server=" << (config.is_server ? "1" : "0") << "\n";
    f << "use_wine=" << (config.use_wine ? "1" : "0") << "\n";
}
