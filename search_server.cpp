#include "search_server.h"

using namespace std::string_literals;

void SearchServer::AddDocument(int document_id, const std::string_view document, DocumentStatus status,
    const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("document_id must be positive"s);
    }
    if (!IsValidWord(document)) {
        throw std::invalid_argument("invalid characters in document's text"s);
    }
    if (documents_.count(document_id) > 0) {
        throw std::invalid_argument("this document_id already exists"s);
    }

    documents_texts_.push_back(std::string{document.begin(), document.end()});
    const std::vector<std::string_view> words = SplitIntoWordsNoStop(documents_texts_.back());
    const double inv_word_count = 1.0 / words.size();
    for (const std::string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        words_freqs_in_document[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    id_list_.insert(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    //remove from documents_
    documents_.erase(document_id);

    //remove from word_to_document_freqs_
    for (const auto& [word, id] : words_freqs_in_document.at(document_id))
    {
        word_to_document_freqs_.at(word).erase(document_id);
    }

    //remove from words_freqs_in_document
    words_freqs_in_document.erase(document_id);

    //remove from id_list_
    id_list_.erase(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, status);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const
{
    const Query query = ParseQuery(raw_query);

    std::vector<std::string_view> matched_words;

    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return std::pair{ std::vector<std::string_view>{}, documents_.at(document_id).status };
        }
    }

    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    
    return std::pair{ matched_words, documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy policy, const std::string_view raw_query, int document_id) const
{
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy policy, const std::string_view raw_query, int document_id) const
{
    const Query query = ParseQuery(std::execution::par, raw_query);

    if (std::any_of(std::execution::par,
        query.minus_words.cbegin(), query.minus_words.cend(),
        [this, document_id](const auto& word) { return word_to_document_freqs_.at(word).count(document_id) > 0; }
    )) {
        return std::pair{ std::vector<std::string_view>{}, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words(query.plus_words.size());

    auto last = std::copy_if(std::execution::par,
        query.plus_words.cbegin(), query.plus_words.cend(),
        matched_words.begin(),
        [this, document_id](const auto& word) { return word_to_document_freqs_.at(word).count(document_id) > 0; }
    );
    matched_words.erase(last, matched_words.end());
    std::sort(std::execution::par, matched_words.begin(), matched_words.end());
    matched_words.erase(std::unique(matched_words.begin(), matched_words.end()), matched_words.end());

    return std::pair{ matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsValidWord(const std::string_view word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
    std::vector<std::string_view> words;
    for (const std::string_view word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text[0] == '-' || text.empty()) {
        throw std::invalid_argument("incorrect spelling of minus-words"s);
    }
    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view text) const {

    auto query = ParseQuery(std::execution::par, text);

    std::sort(query.minus_words.begin(), query.minus_words.end());
    query.minus_words.erase(std::unique(query.minus_words.begin(), query.minus_words.end()), query.minus_words.end());
    std::sort(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.erase(std::unique(query.plus_words.begin(), query.plus_words.end()), query.plus_words.end());

    return query;
}

SearchServer::Query SearchServer::ParseQuery(const std::execution::parallel_policy policy, const std::string_view text) const {
    if (!IsValidWord(text)) {
        throw std::invalid_argument("invalid characters in query"s);
    }
    Query query;
    for (const std::string_view word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
            else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }

    return query;
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<std::string_view, double> dummy_map;
    if (words_freqs_in_document.count(document_id) == 0) return dummy_map;
    return words_freqs_in_document.at(document_id);
}