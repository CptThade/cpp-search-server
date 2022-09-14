#pragma once
#include "search_server.h"
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : search_server_(search_server)
        , r_no_results_(0)
        , curr_time_(0) {
    }

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
        AddRequest(result.size());
        return result;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status) {
        const auto result = search_server_.FindTopDocuments(raw_query, status);
        AddRequest(static_cast<int>(result.size()));
        return result;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query) {
        const auto result = search_server_.FindTopDocuments(raw_query);
        AddRequest(static_cast<int>(result.size()));
        return result;
    }

    int GetNoResultRequests() const {
        return r_no_results_;
    }

private:
    struct QueryResult {
        uint64_t timepoint;
        int results;
    };
    std::deque<QueryResult> requests_;
    const SearchServer& search_server_;
    int r_no_results_;
    uint64_t curr_time_;
    const static int day_minutes_ = 1440;

    void AddRequest(int n_results) {
        ++curr_time_;
        while (!requests_.empty() && day_minutes_ <= curr_time_ - requests_.front().timepoint) {
            if (requests_.front().results == 0) {
                --r_no_results_;
            }
            requests_.pop_front();
        }
        requests_.push_back({ curr_time_, n_results });
        if (n_results == 0) {
            ++r_no_results_;
        }
    }
};