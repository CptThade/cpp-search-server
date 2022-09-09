//Журавлев Никита Сергеевич
//07.09.2022

#include <string>
#include <vector>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <utility>

using namespace std;
//Поисковый сервер

const int MAX_RESULT_DOCUMENT_COUNT = 5;

vector<string> SplitIntoWords(const string& text) {
	vector<string> words;
	string word;
	for (const char c : text) {
		if (c == ' ') {
			if (!word.empty()) {
				words.push_back(word);
				word.clear();
			}
		}
		else {
			word += c;
		}
	}
	if (!word.empty()) {
		words.push_back(word);
	}

	return words;
}

struct Document {
	int id;
	double relevance;
	int rating;
};

enum class DocumentStatus {
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED,
};

class SearchServer {
public:
	void SetStopWords(const string& text) {
		for (const string& word : SplitIntoWords(text)) {
			stop_words_.insert(word);
		}
	}

	void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
		const vector<string> words = SplitIntoWordsNoStop(document);
		const double inv_word_count = 1.0 / words.size();
		for (const string& word : words) {
			word_to_document_freqs_[word][document_id] += inv_word_count;
		}
		documents_.emplace(document_id,
			DocumentData{
				ComputeAverageRating(ratings),
				status
			});
	}

	template<typename FPredicate>
	vector<Document> FindTopDocuments(const string& raw_query, const FPredicate& fpredicate) const {
		const Query query = ParseQuery(raw_query);
		auto matched_documents = FindAllDocuments(query, fpredicate);

		sort(matched_documents.begin(), matched_documents.end(),
			[](const Document& lhs, const Document& rhs) {
				if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
					return lhs.rating > rhs.rating;
				}
				else {
					return lhs.relevance > rhs.relevance;
				}
			});
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}
		return matched_documents;
	}

	vector<Document> FindTopDocuments(const string& raw_query, const DocumentStatus& document_status) const {
		return FindTopDocuments(raw_query, [&document_status](int document_id, DocumentStatus status, int rating) { return status == document_status; });
	}

	vector<Document> FindTopDocuments(const string& raw_query) const {

		return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
	}

	int GetDocumentCount() const {
		return documents_.size();
	}

	tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
		const Query query = ParseQuery(raw_query);
		vector<string> matched_words;
		for (const string& word : query.plus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			if (word_to_document_freqs_.at(word).count(document_id)) {
				matched_words.push_back(word);
			}
		}
		for (const string& word : query.minus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			if (word_to_document_freqs_.at(word).count(document_id)) {
				matched_words.clear();
				break;
			}
		}
		return { matched_words, documents_.at(document_id).status };
	}

private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
	};

	set<string> stop_words_;
	map<string, map<int, double>> word_to_document_freqs_;
	map<int, DocumentData> documents_;

	bool IsStopWord(const string& word) const {
		return stop_words_.count(word) > 0;
	}

	vector<string> SplitIntoWordsNoStop(const string& text) const {
		vector<string> words;
		for (const string& word : SplitIntoWords(text)) {
			if (!IsStopWord(word)) {
				words.push_back(word);
			}
		}
		return words;
	}

	static int ComputeAverageRating(const vector<int>& ratings) {
		if (ratings.empty()) {
			return 0;
		}
		int rating_sum = 0;
		for (const int rating : ratings) {
			rating_sum += rating;
		}
		return rating_sum / static_cast<int>(ratings.size());
	}

	struct QueryWord {
		string data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(string text) const {
		bool is_minus = false;
		if (text[0] == '-') {
			is_minus = true;
			text = text.substr(1);
		}
		return {
			text,
			is_minus,
			IsStopWord(text)
		};
	}

	struct Query {
		set<string> plus_words;
		set<string> minus_words;
	};

	Query ParseQuery(const string& text) const {
		Query query;
		for (const string& word : SplitIntoWords(text)) {
			const QueryWord query_word = ParseQueryWord(word);
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

	double ComputeWordInverseDocumentFreq(const string& word) const {
		return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
	}

	template<typename FPredicate>
	vector<Document> FindAllDocuments(const Query& query, const FPredicate& fpredicate) const {
		map<int, double> document_to_relevance;
		for (const string& word : query.plus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
			for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
				if (fpredicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
					document_to_relevance[document_id] += term_freq * inverse_document_freq;
				}
			}
		}

		for (const string& word : query.minus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
				document_to_relevance.erase(document_id);
			}
		}

		vector<Document> matched_documents;
		for (const auto [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back({
				document_id,
				relevance,
				documents_.at(document_id).rating
				});
		}
		return matched_documents;
	}
};

//Тесты и макросы

template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container) {
	bool is_first = true;
	out << "["s;
	for (const Element& element : container) {
		if (!is_first) {
			out << ", "s << element;
		}
		else {
			out << element;
			is_first = false;
		}

	}
	out << "]"s;
	return out;
}

template <typename Element>
ostream& operator<<(ostream& out, const set<Element>& container) {
	bool is_first = true;
	out << "{"s;
	for (const Element& element : container) {
		if (!is_first) {
			out << ", "s << element;
		}
		else {
			out << element;
			is_first = false;
		}

	}
	out << "}"s;
	return out;
}

template <typename KeyElement, typename ValueElement>
ostream& operator<<(ostream& out, const map<KeyElement, ValueElement>& container) {
	bool is_first = true;
	out << "{"s;
	for (const auto& [key, value] : container) {
		if (!is_first) {
			out << ", "s << key << ": "s << value;
		}
		else {
			out << key << ": "s << value;
			is_first = false;
		}

	}
	out << "}"s;
	return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
	const string& func, unsigned line, const string& hint) {
	if (t != u) {
		cerr << boolalpha;
		cerr << file << "("s << line << "): "s << func << ": "s;
		cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
		cerr << t << " != "s << u << "."s;
		if (!hint.empty()) {
			cerr << " Hint: "s << hint;
		}
		cerr << endl;
		abort();
	}
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
	const string& hint) {
	if (!value) {
		cerr << file << "("s << line << "): "s << func << ": "s;
		cerr << "ASSERT("s << expr_str << ") failed."s;
		if (!hint.empty()) {
			cerr << " Hint: "s << hint;
		}
		cerr << endl;
		abort();
	}
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
	const int doc_id = 42;
	string content = "cat in the city"s;
	vector<int> ratings = { 1, 2, 3 };
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("in"s);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
	}

	{
		SearchServer server;
		server.SetStopWords("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
	}
}

//Добавление документов.Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.

void TestSearchResultFromAddedDocumentContent() {
	SearchServer server;

	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };

	server.SetStopWords("in the"s);
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

	{
		const int doc_id = 53;
		const string content = "cat city in the bat"s;
		const vector<int> ratings = { 1, 2, 3 };

		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	}

	const auto found_docs = server.FindTopDocuments(content);
	ASSERT_EQUAL(found_docs.at(0).id, doc_id);
}

//Поддержка стоп-слов. Стоп-слова исключаются из текста документов.

void TestStopWordsSupport() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };

	SearchServer server;
	server.SetStopWords("in the"s);
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	const auto found_docs = server.FindTopDocuments("in"s);
	ASSERT_EQUAL(found_docs.size(), 0);

}


//Поддержка минус-слов. Документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска.

void TestExcludeMinusWords() {
	SearchServer server;

	int doc_id = 42;
	string content = "cat in the city"s;
	vector<int> ratings = { 1, 2, 3 };

	server.SetStopWords("in the"s);
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);


	doc_id = 53;
	content = "cat in the bat"s;
	ratings = { 1, 2, 3 };

	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

	const auto found_docs = server.FindTopDocuments("cat -city"s);
	ASSERT_EQUAL(found_docs.size(), 1);
	const Document& doc0 = found_docs[0];
	ASSERT_EQUAL(doc0.id, 53);
}

//Матчинг документов. При матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса, присутствующие в документе.
//Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.

void TestDocumentMatching() {
	SearchServer server;

	int doc_id = 42;
	string content = "cat in the city"s;
	vector<int> ratings = { 1, 2, 3 };

	server.SetStopWords("in the"s);
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

	doc_id = 53;
	content = "cat in the bat"s;
	ratings = { 1, 2, 3 };

	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

	const auto [documents, _] = server.MatchDocument("cat -city"s, 53);
	const string hint = "Check your mutcher. Something gone wrong";
	const vector<string> test_document = { "cat"s };
	ASSERT_EQUAL_HINT(documents, test_document, hint);

	{
		const auto [documents, _] = server.MatchDocument("-cat -city"s, 42);
		ASSERT(documents.empty());
	}
}

//Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.

void TestFindAndSortByRelevance() {
	SearchServer server;

	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };

	server.SetStopWords("in the"s);
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

	{
		const int doc_id = 53;
		const string content = "cat in the bat"s;
		const vector<int> ratings = { 1, 2, 3 };

		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	}

	{
		const int doc_id = 71;
		const string content = "cat in the city within bat"s;
		const vector<int> ratings = { 1, 2, 3 };

		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	}

	const auto found_docs = server.FindTopDocuments(content);
	ASSERT(found_docs.at(0).id == 42);
	ASSERT(found_docs.at(1).id == 71);
	ASSERT(found_docs.at(2).id == 53);

}

//Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestCountRating() {
	SearchServer server;

	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 4, 5, 7 };

	server.SetStopWords("in the"s);
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	const auto found_docs = server.FindTopDocuments(content);

	int precalculated_rate = 0;
	for (const int& rate : ratings) {
		precalculated_rate += rate;
	}

	ASSERT_HINT(found_docs.at(0).rating == (precalculated_rate / 3), "something wrong with rating calculation"s);
}

//Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
void TestSearchByPredicate() {
	SearchServer server;

	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };

	server.SetStopWords("in the"s);
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

	{
		const int doc_id = 53;
		const string content = "cat in the bat"s;
		const vector<int> ratings = { 1, 2, 3 };

		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	}

	{
		const int doc_id = 71;
		const string content = "cat in the city within bat"s;
		const vector<int> ratings = { 1, 2, 3 };

		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	}

	const auto found_docs = server.FindTopDocuments(content, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
	const string hint = "check your predicate executing"s;
	ASSERT_HINT(found_docs.size() == 1, hint);
	ASSERT_HINT(found_docs.at(0).id == 42, hint);
}
//Поиск документов, имеющих заданный статус.

void TestFindDocumentByStatus() {
	SearchServer server;

	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };

	server.SetStopWords("in the"s);
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

	{
		const int doc_id = 53;
		const string content = "cat in the bat"s;
		const vector<int> ratings = { 1, 2, 3 };

		server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
	}

	{
		const int doc_id = 71;
		const string content = "cat in the city within bat"s;
		const vector<int> ratings = { 1, 2, 3 };

		server.AddDocument(doc_id, content, DocumentStatus::REMOVED, ratings);
	}

	{
		const int doc_id = 84;
		const string content = "cat in the cifedfety withiaffsdfsfgtgen bat"s;
		const vector<int> ratings = { 1, 2, 3 };

		server.AddDocument(doc_id, content, DocumentStatus::REMOVED, ratings);
	}

	const auto found_ban_docs = server.FindTopDocuments(content, DocumentStatus::BANNED);
	ASSERT(found_ban_docs.size() == 1);
	ASSERT(found_ban_docs.at(0).id == 53);

	const auto found_rem_docs = server.FindTopDocuments(content, DocumentStatus::REMOVED);
	ASSERT(found_rem_docs.size() == 2);
	ASSERT(found_rem_docs.at(0).id == 71);
}

//Корректное вычисление релевантности найденных документов.

void TestCountCorrectRelevance() {
	SearchServer server;

	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };

	server.SetStopWords("in the"s);
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

	{
		const int doc_id = 53;
		const string content = "cat in the bat"s;
		const vector<int> ratings = { 1, 2, 3 };

		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	}

	{
		const int doc_id = 71;
		const string content = "cat in the city within bat"s;
		const vector<int> ratings = { 1, 2, 3 };

		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	}

	const auto found_docs = server.FindTopDocuments(content);
	ASSERT(abs(found_docs.at(0).relevance - 0.20273255405408219) < 1e-6);
	ASSERT(abs(found_docs.at(1).relevance - 0.10136627702704110) < 1e-6);
	ASSERT(abs(found_docs.at(2).relevance - 0.0000000000000000) < 1e-6);
}

template <typename T, typename U>
void RunTestImpl(const T& t, const U& u) {
	t();
	cerr << u << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func);

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
	RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
	RUN_TEST(TestSearchResultFromAddedDocumentContent);
	RUN_TEST(TestStopWordsSupport);
	RUN_TEST(TestExcludeMinusWords);
	RUN_TEST(TestDocumentMatching);
	RUN_TEST(TestFindAndSortByRelevance);
	RUN_TEST(TestCountRating);
	RUN_TEST(TestSearchByPredicate);
	RUN_TEST(TestFindDocumentByStatus);
	RUN_TEST(TestCountCorrectRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
	TestSearchServer();
	// Если вы видите эту строку, значит все тесты прошли успешно
	cout << "Search server testing finished"s << endl;
}