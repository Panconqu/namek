#ifndef NAMEK_MOD_MARKETPLACE_H
#define NAMEK_MOD_MARKETPLACE_H

#include <string>
#include <vector>

namespace namek {

struct RemoteModInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string download_url;
    std::string checksum;
};

class ModMarketplace {
private:
    static std::string default_registry_url;

public:
    // Fetches remote mod registry from GitHub repository
    static std::vector<RemoteModInfo> fetch_remote_registry(const std::string& registry_url = "");

    // Interactive GitHub Mod Search & Display in CLI
    static void show_remote_mods();

    // Direct automated download of Lua mod from GitHub into mods/ folder
    static bool install_remote_mod(const std::string& mod_name, bool auto_confirm = false);
};

} // namespace namek

#endif // NAMEK_MOD_MARKETPLACE_H
