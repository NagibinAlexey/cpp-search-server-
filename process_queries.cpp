#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries)
{
	std::vector<std::vector<Document>> documents_lists(queries.size());
	std::transform(
		std::execution::par,
		queries.cbegin(), queries.cend(),
		documents_lists.begin(),
		[&search_server](const std::string& query) { return search_server.FindTopDocuments(query); });
	return documents_lists;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    std::list<Document> documents;
    for (const auto& docs : ProcessQueries(search_server, queries)) 
    {
        for (const auto& doc : docs) 
        {
            documents.push_back(doc);
        }
    } 
    return documents;
}