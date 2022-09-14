#include "document.h"

std::ostream& operator<<(std::ostream& out, const Document& document) {
	using namespace std::string_literals;

	out << "{ "s
		<< "document_id = "s << document.id << ", "s
		<< "relevance = "s << document.relevance << ", "s
		<< "rating = "s << document.rating << " }"s;
	return out;
}

void PrintDocument(const Document& document) {
	using namespace std::string_literals;
	std::cout << document;
}

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status) {
	using namespace std::string_literals;
	std::cout << "{ "s
		<< "document_id = "s << document_id << ", "s
		<< "status = "s << static_cast<int>(status) << ", "s
		<< "words ="s;
	for (const std::string& word : words) {
		std::cout << ' ' << word;
	}
	std::cout << "}"s << std::endl;
}