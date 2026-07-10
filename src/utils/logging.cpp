#include <string>
#include "logging.h"

namespace sample {

    std::string log_path("");

    /**
     * @brief Return the optional file path where stream logs are mirrored.
     */
    const std::string &get_log_file_path() {
        return log_path;
    }

    /**
     * @brief Update the optional file path where stream logs are mirrored.
     * @param path Destination log file path.
     */
    void set_log_file_path(const std::string &path) {
        log_path = path;
    }
}
