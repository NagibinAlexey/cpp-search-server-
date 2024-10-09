#pragma once

#include "search_server.h"
#include <vector>
#include <string>
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) : search_server_(search_server),
        current_time(0), no_answer_requests(0) {}

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const { return no_answer_requests; }

private:
    const SearchServer& search_server_;
    struct QueryResult {
        std::vector<Document> matched_documents;
        int documents_count;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int current_time;
    int no_answer_requests;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    ++current_time;
    if (current_time > min_in_day_)
    {
        if (requests_.front().documents_count == 0)
        {
            --no_answer_requests;
        }
        requests_.pop_front();
    }
    auto matched_documents = search_server_.FindTopDocuments(raw_query, document_predicate);

    requests_.push_back({ matched_documents, static_cast<int>(matched_documents.size()) });
    if (requests_.back().documents_count == 0)
    {
        ++no_answer_requests;
    }

    return matched_documents;
}
