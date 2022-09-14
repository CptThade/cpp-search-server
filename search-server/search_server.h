#pragma once
#include "string_processing.h"
#include "document.h"
#include <map>
#include <algorithm>
#include <cmath>
#include <numeric>


const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:

	inline static constexpr int INVALID_DOCUMENT_ID = -1;

	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words)
		: stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
		for (const std::string& word : stop_words_) {
			if (!IsValidWord(word)) {
				using namespace std::string_literals;
				throw std::invalid_argument("word contains invalid characters: "s + word);
			}
		}
	}

	explicit SearchServer(const std::string& stop_words_text)
		: SearchServer(SplitIntoWords(stop_words_text)) {
	}

	void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const;

	std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;

	std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

	int GetDocumentCount() const;

	int GetDocumentId(int index) const;

	std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;
private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
	};
	const std::set<std::string> stop_words_;
	std::map<std::string, std::map<int, double>> word_to_document_freqs_;
	std::map<int, DocumentData> documents_;
	std::vector<int> document_ids_;

	bool IsStopWord(const std::string& word) const {
		return stop_words_.count(word) > 0;
	}

	static bool IsValidWord(const std::string& word) {
		return none_of(word.begin(), word.end(), [](char c) {
			return c >= '\0' && c < ' ';
			});
	}

	std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const {
		using namespace std::string_literals;
		std::vector<std::string> words;
		for (const std::string& word : SplitIntoWords(text)) {
			if (!IsValidWord(word)) {
				throw std::invalid_argument("word contains invalid characters: "s + word);
			}
			if (!IsStopWord(word)) {
				words.push_back(word);
			}
		}
		return words;
	}

	static int ComputeAverageRating(const std::vector<int>& ratings) {
		if (ratings.empty()) {
			return 0;
		}
		int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
		return rating_sum / static_cast<int>(ratings.size());
	}

	struct QueryWord {
		std::string data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(std::string text) const {
		using namespace std::string_literals;
		if (text.empty()) {
			throw std::invalid_argument("text empty"s);
		}
		bool is_minus = false;
		if (text[0] == '-') {
			is_minus = true;
			text = text.substr(1);
		}
		if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
			throw std::invalid_argument("text contains invalid characters: "s + text);
		}

		return QueryWord{ text, is_minus, IsStopWord(text) };

	}

	struct Query {
		std::set<std::string> plus_words;
		std::set<std::string> minus_words;
	};

	Query ParseQuery(const std::string& text) const {
		Query query;
		for (const std::string& word : SplitIntoWords(text)) {
			QueryWord query_word = ParseQueryWord(word);
			if (!query_word.is_stop) {
				if (query_word.is_minus) {
					query.minus_words.insert(query_word.data);
				}
				else {
					query.plus_words.insert(query_word.data);
				}
			}
		}
		return query;
	}

	// Existence required
	double ComputeWordInverseDocumentFreq(const std::string& word) const {
		return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
	}

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
		std::map<int, double> document_to_relevance;
		for (const std::string& word : query.plus_words) {
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

		for (const std::string& word : query.minus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
				document_to_relevance.erase(document_id);
			}
		}

		std::vector<Document> matched_documents;
		for (const auto [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
		}
		return matched_documents;
	}
};

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const {
	Query query = ParseQuery(raw_query);

	auto matched_documents = FindAllDocuments(query, document_predicate);

	sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
		const double err_rate = 1e-6;
		if (std::abs(lhs.relevance - rhs.relevance) < err_rate) {
			return lhs.rating > rhs.rating;
		}
			return lhs.relevance > rhs.relevance;
		});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}

	return matched_documents;
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
	const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);