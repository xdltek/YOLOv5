#pragma once

#include <cstdlib>
#include <string>
#include <vector>

/**
 * @brief Expand a leading `~` in a user-provided path to the current HOME directory.
 * @param path CLI path value before filesystem validation.
 */
inline std::string expand_user_path(const std::string& path)
{
    // Only a leading tilde is treated as a shell-style home shortcut.
    if (!path.empty() && path[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home) {
            return home + path.substr(1);
        }
    }
    return path;
}

/**
 * @brief Normalize short `-x=value` CLI arguments into `-x value` for argparse.
 * @param argc Number of original command-line arguments.
 * @param argv Original command-line argument values.
 */
inline std::vector<std::string> preprocess_args(int argc, char* argv[])
{
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // Split only short options; long options are already handled by argparse.
        if (arg.size() > 2 && arg[0] == '-' && arg[1] != '-' && arg.find('=') != std::string::npos) {
            size_t pos = arg.find('=');
            std::string flag = arg.substr(0, pos);
            std::string value = arg.substr(pos + 1);

            args.push_back(flag);
            args.push_back(value);
        } else {
            args.push_back(arg);
        }
    }
    return args;
}

/**
 * @brief Build the argv-compatible pointer list consumed by argparse.
 * @param preprocessed_arguments Stable normalized argument strings.
 * @param original_argv Original argv array, used to preserve program name.
 * @param fixed_arguments Output pointer list passed to ArgumentParser::parse_args.
 */
inline void to_char_argument_vector(const std::vector<std::string>& preprocessed_arguments, char* original_argv[],
                                    std::vector<const char*>& fixed_arguments)
{
    fixed_arguments.clear();
    // Keep argv[0] as the executable name expected by CLI parsers.
    fixed_arguments.push_back(original_argv[0]);
    for (auto& preprocessed_argument : preprocessed_arguments) {
        fixed_arguments.push_back(preprocessed_argument.data());
    }
}
