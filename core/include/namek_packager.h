#ifndef NAMEK_PACKAGER_H
#define NAMEK_PACKAGER_H

#include <string>

namespace namek {

struct PackOptions {
    std::string dist_path = "dist";
    std::string bundle_name = "Namek Release Bundle";
    std::string release_key = "";   // empty -> generate a fresh random key
    int mv_layers = 5;              // MV obfuscation layers for the runtime runners
    bool skip_runtimes = false;     // skip generating dist/mv_runtimes
    bool verbose = true;
};

class Packager {
public:
    // Builds full multi-folder release package (trio .tb.bin + MV runners +
    // native launchers + manifest). Returns true on success.
    static bool build_release_package(const PackOptions& options);
};

} // namespace namek

#endif // NAMEK_PACKAGER_H
