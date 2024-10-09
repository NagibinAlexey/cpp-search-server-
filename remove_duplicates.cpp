#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
	std::vector<int> ids_for_remove;
	std::set<std::set<std::string_view>> documents_words;

	for (const int document_id : search_server) {
		auto words = search_server.GetWordFrequencies(document_id);
		std::set<std::string_view> document_words;
		for (const auto& [word, freq] : words) {
			document_words.insert(word);
		}

		if (documents_words.count(document_words) > 0) {
			ids_for_remove.push_back(document_id);
		}
		else {
			documents_words.insert(document_words);
		}
	}

	for (const int id : ids_for_remove) {
		std::cout << "Found duplicate document id " << id << std::endl;
		search_server.RemoveDocument(id);
	}
}