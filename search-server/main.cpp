//Журавлев Никита Сергеевич
//07.09.2022

#include "search_server.h"
#include "custom_asserts.h"
#include <algorithm>
#include <string>
#include <vector>

using namespace std;

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
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

void TestMinusWordsExclude() {
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
    const auto found_docs = server.FindTopDocuments("cat -city"s);
    ASSERT_EQUAL(found_docs.size(), 1);
    const Document& doc0 = found_docs[0];
    ASSERT_EQUAL(doc0.id, 53);
}

//Матчинг документов. При матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса, присутствующие в документе.
//Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.

void TestDocumentMatching() {
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
    const auto [documents, _] = server.MatchDocument("cat - city"s, 53);
    const string hint = "Check your mutcher. Something gone wrong";
    ASSERT_EQUAL_HINT(count(documents.begin(), documents.end(), "cat"s),1 , hint);
    ASSERT_EQUAL_HINT(count(documents.begin(), documents.end(), "city"s),0 , hint);
    {
        const auto [documents, _] = server.MatchDocument("-cat - city"s, 42);
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
void TestRatingCounting() {
    SearchServer server;

    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 4, 5, 7 };

    server.SetStopWords("in the"s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments(content);
    ASSERT_HINT(found_docs.at(0).rating == 5, "something wrong with rating calculation"s);
}

//Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
void TestUsingPredicate() {
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

void TestFindByDocumentStatus() {
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
    const auto found_docs = server.FindTopDocuments(content, DocumentStatus::BANNED);
    ASSERT(found_docs.size() == 1);
    ASSERT(found_docs.at(0).id == 53);
    {
        const auto found_docs = server.FindTopDocuments(content, DocumentStatus::REMOVED);
        ASSERT(found_docs.size() == 2);
        ASSERT(found_docs.at(0).id == 71);
    }
}

//Корректное вычисление релевантности найденных документов.

void TestCorrectRelevanceCounting() {
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
    ASSERT(found_docs.at(0).relevance == 0.20273255405408219);
    ASSERT(found_docs.at(1).relevance == 0.10136627702704110);
    ASSERT(found_docs.at(2).relevance == 0.0000000000000000);
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
    RUN_TEST(TestMinusWordsExclude);
    RUN_TEST(TestDocumentMatching);
    RUN_TEST(TestFindAndSortByRelevance);
    RUN_TEST(TestRatingCounting);
    RUN_TEST(TestUsingPredicate);
    RUN_TEST(TestFindByDocumentStatus);
    RUN_TEST(TestCorrectRelevanceCounting);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}