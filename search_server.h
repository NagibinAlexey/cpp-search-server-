#pragma once

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <set>
#include <map>
#include <tuple>
#include <utility>
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <execution>
#include <type_traits>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double DELTA = 1e-6;

class SearchServer {
public:
    // Defines an invalid document id
    // You can refer to this constant as SearchServer::INVALID_DOCUMENT_ID
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(std::string_view(stop_words_text)) {} // delegating constructor for string_view constructor
    explicit SearchServer(const std::string_view stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {} // Invoke delegating constructor from string container

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status,
        const std::vector<int>& ratings);

    template <class ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);
    void RemoveDocument(int document_id);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query,
        DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    template <class ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query,
        DocumentPredicate document_predicate) const;

    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentStatus status) const;

    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query) const;

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy policy, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy policy, const std::string_view raw_query, int document_id) const;

    const auto begin() const {
        return id_list_.begin();
    }

    const auto end() const {
        return id_list_.end();
    }

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::set<int> id_list_;
    const std::set<std::string, std::less<>> stop_words_;
    std::deque<std::string> documents_texts_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> words_freqs_in_document;
    std::map<int, DocumentData> documents_;

    static bool IsValidWord(const std::string_view word);

    bool IsStopWord(const std::string_view word) const {
        return stop_words_.count(word) > 0;
    }

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view text) const;
    Query ParseQuery(const std::execution::parallel_policy policy, const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const {
        return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <class ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(ExecutionPolicy&& policy, const Query& query,
        DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy policy, const Query& query,
        DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    using namespace std::string_literals;
    if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("invalid characters in stop_words"s);
    }
}

template <class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
    if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy>) {
        RemoveDocument(document_id);
    } else {
        //remove from documents_
        documents_.erase(document_id);

        //remove from word_to_document_freqs_
        if (words_freqs_in_document.count(document_id) > 0) {
            std::map<std::string_view, double>& words_freqs = words_freqs_in_document.at(document_id);
            std::vector<std::string_view*> words_to_delete(words_freqs.size());
            std::transform(policy,
                words_freqs.cbegin(), words_freqs.cend(),
                words_to_delete.begin(),
                [](const auto& word) { return const_cast<std::string_view*>(&word.first); });
            std::for_each(policy,
                words_to_delete.cbegin(), words_to_delete.cend(),
                [this, document_id](const auto& word) { word_to_document_freqs_.at(*word).erase(document_id); }
            );
        }

        //remove from words_freqs_in_document
        words_freqs_in_document.erase(document_id);

        //remove from id_list_
        id_list_.erase(document_id);
    } 
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query,
    DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <class ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query,
    DocumentPredicate document_predicate) const {
    auto matched_documents = FindAllDocuments(policy, ParseQuery(raw_query), document_predicate);
    std::sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < DELTA) {
                return lhs.rating > rhs.rating;
            }
            return lhs.relevance > rhs.relevance;
        });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        policy, raw_query, [status](int document_id, DocumentStatus document_status,
            int rating) {
                return document_status == status;
        });
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <class ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, const Query& query,
                                      DocumentPredicate document_predicate) const {
        std::map<int, double> document_to_relevance;
        for (const std::string_view word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
        for (const std::string_view word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
 
        std::vector<Document> matched_documents;

        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }

    template <typename DocumentPredicate>
    std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy policy, const Query& query,
        DocumentPredicate document_predicate) const {
        constexpr size_t THREAD_COUNT = 64;
        ConcurrentMap<int, double> doc_to_rel_cm(THREAD_COUNT);

        auto PlusWordFreqs = [&](const std::string_view word) {
                if (word_to_document_freqs_.count(word) == 0) {
                    return;
                }
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    if (document_predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                        doc_to_rel_cm[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
        };

        std::for_each(policy, query.plus_words.begin(), query.plus_words.end(), PlusWordFreqs);

        std::for_each(policy, query.minus_words.cbegin(), query.minus_words.cend(),
            [&](const std::string_view word) {
                if (word_to_document_freqs_.count(word) != 0) {
                    for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                        doc_to_rel_cm.Erase(document_id);
                    }
                }
            }
        );

        std::map<int, double> document_to_relevance = doc_to_rel_cm.BuildOrdinaryMap();

        std::vector<Document> matched_documents(document_to_relevance.size());
        std::transform(policy, 
            document_to_relevance.cbegin(), document_to_relevance.cend(),
            matched_documents.begin(),
            [this](const auto& doc) { return Document{ doc.first, doc.second, documents_.at(doc.first).rating }; });

        return matched_documents;
    }
