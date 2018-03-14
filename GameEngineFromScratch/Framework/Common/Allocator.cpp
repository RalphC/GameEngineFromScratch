#include <cassert>
#include <cstring>

#include "Allocator.hpp"

#ifndef ALIGN
#define ALIGN(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#endif

using namespace My;

My::Allocator::Allocator(size_t dataSize, size_t pageSize, size_t alignment)
	: m_pageList(nullptr)
	, m_freeList(nullptr) {
	Reset(dataSize, pageSize, alignment);
}

Allocator::~Allocator(void) {
	FreeAll();
}

void Allocator::Reset(size_t dataSize, size_t pageSize, size_t alignment) {
	FreeAll();

	m_dataSize = dataSize;
	m_pageSize = pageSize;
	m_alignmentSize = alignment;



	size_t minimun_size = sizeof(BlockHeader) > m_dataSize ? sizeof(BlockHeader) : m_dataSize;

#if defined(_DEBUG)
	assert(alignment > 0 && (alignment & (alignment - 1)) == 0);
#endif

	m_blockSize = ALIGN(minimun_size, alignment);

	m_alignmentSize = m_blockSize - minimun_size;
	m_blockPerPage = (m_pageSize - sizeof(PageHeader)) / m_blockSize;
}

void* Allocator::Allocate() {
	if (!m_freeList) {
		PageHeader* pNewPage = reinterpret_cast<PageHeader*>(new uint8_t[m_pageSize]);
		++m_numPages;
		m_numBlocks += m_blockPerPage;
		m_numFreeBlocks += m_blockPerPage;

#if defined(_DEBUG)
		FillFreePage(pNewPage);
#endif

		if (m_pageList) {
			pNewPage->pNext = m_pageList;
		}

		m_pageList = pNewPage;

		BlockHeader* pBlock = pNewPage->Blocks();
		for (uint32_t i = 0; i < m_blockPerPage; i++) {
			pBlock->pNext = NextBlock(pBlock);
			pBlock = NextBlock(pBlock);
		}

		pBlock->pNext = nullptr;

		m_freeList = pNewPage->Blocks();
	}

	BlockHeader* freeBlock = m_freeList;
	m_freeList = m_freeList->pNext;
	--m_numFreeBlocks;

#if defined(_DEBUG)
	FillAllocatedBlock(freeBlock);
#endif

	return reinterpret_cast<void*>(freeBlock);
}

void Allocator::Free(void* p) {
	BlockHeader* pBlock = reinterpret_cast<BlockHeader*>(p);

#if defined(_DEBUG)
	FillFreeBlock(pBlock);
#endif

	pBlock->pNext = m_freeList;
	m_freeList = pBlock;
	++m_numFreeBlocks;
}

void Allocator::FreeAll() {
	PageHeader* pPage = m_pageList;

	while (pPage) {
		PageHeader* p = pPage;
		pPage = pPage->pNext;

		delete[] reinterpret_cast<uint8_t*>(p);
	}

	m_pageList = nullptr;
	m_freeList = nullptr;

	m_numBlocks = 0;
	m_numPages = 0;
	m_numFreeBlocks = 0;
}

#if defined(_DEBUG)

void Allocator::FillFreePage(PageHeader* pPage) {
	pPage->pNext = nullptr;

	BlockHeader* pBlock = pPage->Blocks();
	for (uint32_t i = 0; i < m_blockPerPage; i++) {
		FillFreeBlock(pBlock);
		pBlock = NextBlock(pBlock);
	}
}

void Allocator::FillFreeBlock(BlockHeader* pBlock) {
	std::memset(pBlock, PATTERN_FREE, m_blockSize - m_alignmentSize);

	std::memset(reinterpret_cast<uint8_t*>(pBlock) + m_blockSize - m_alignmentSize, PATTERN_ALIGN, m_alignmentSize);
}

void Allocator::FillAllocatedBlock(BlockHeader* pBlock) {
	std::memset(pBlock, PATTERN_ALLOC, m_blockSize - m_alignmentSize);

	std::memset(reinterpret_cast<uint8_t*>(pBlock) + m_blockSize - m_alignmentSize, PATTERN_ALIGN, m_alignmentSize);
}

#endif

BlockHeader* Allocator::NextBlock(BlockHeader* pBlock) {
	return reinterpret_cast<BlockHeader*>(reinterpret_cast<uint8_t*>(pBlock) + m_blockSize);
}
