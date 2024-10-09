#pragma once

#include <iostream>
#include <iterator>

template <typename It>
class IteratorRange {
public:
    IteratorRange(It begin, It end)
        : begin_(begin), end_(end) {
    }

    It begin() const {
        return begin_;
    }
    It end() const {
        return end_;
    }
    size_t size() const {
        return std::distance(begin_, end_);
    }

private:
    It begin_;
    It end_;
};

template <typename It>
std::ostream& operator<<(std::ostream& output, const IteratorRange<It>& iterator_range) {
    for (It it = iterator_range.begin(); it != iterator_range.end(); ++it)
    {
        output << *it;
    }
    return output;
}

template<class Iterator>
class Paginator {
public:
    explicit Paginator(const Iterator begin, const Iterator end, const size_t page_size) {
        size_ = std::distance(begin, end) / page_size + (std::distance(begin, end) % page_size != 0 ? 1 : 0);
        for (int i = 0; i < size_; ++i) {
            Iterator currentBegin = std::next(begin, page_size * i);
            Iterator currentEnd = (i == size_ - 1 ? end : std::next(currentBegin, page_size));
            pages.push_back({ currentBegin, currentEnd });
        }
    }

    size_t size() const {
        return pages.size();
    }

    auto begin() const {
        return pages.begin();
    }

    auto end() const {
        return pages.end();
    }

private:
    size_t size_;
    std::vector<IteratorRange<Iterator>> pages;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

std::ostream& operator<<(std::ostream& output, const Document& document) {
    output << "{ document_id = " << document.id <<
        ", relevance = " << document.relevance <<
        ", rating = " << document.rating << " }";
    return output;
}
