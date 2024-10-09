#include "string_processing.h"

std::vector<std::string_view> SplitIntoWords(const std::string_view text) {
    std::string_view text_ = text;
    std::vector<std::string_view> words;
    text_.remove_prefix(std::min(text_.size(), text_.find_first_not_of(" ")));
    const int64_t pos_end = text_.npos;

    while (!text_.empty()) {
        int64_t space = text_.find(' ', 0);
        words.push_back(space == pos_end ? text_.substr(0) : text_.substr(0, space));
        text_.remove_prefix(std::min(text_.size(), static_cast<size_t>(space)));
        text_.remove_prefix(std::min(text_.size(), text_.find_first_not_of(" ")));
    }
    return words;
}