#pragma once

#include <cstddef>
#include <cstdint>

namespace My
{

	struct BlockHeader
	{
		BlockHeader* pNext;
	};

	struct PageHeader
	{
		PageHeader* pNext;
		BlockHeader* Blocks() {
			return reinterpret_cast<BlockHeader*>(this + 1);
		}
	};

	class Allocator
	{
	public:

		static const unsigned char PATTERN_ALIGN = 0xfc;
		static const unsigned char PATTERN_ALLOC = 0xfd;
		static const unsigned char PATTERN_FREE = 0xfe;

		Allocator();
		Allocator(
			size_t dataSize,
			size_t pageSize,
			size_t alignment
		);

		~Allocator(void);

		void Reset
		(
			size_t dataSize,
			size_t pageSize,
			size_t alignment
		);

		void* Allocate(void);

		void Free(void* p);

		void FreeAll(void);

	private:

		void FillFreePage(PageHeader* p);

		void FillFreeBlock(BlockHeader* p);

		void FillAllocatedBlock(BlockHeader* p);

		BlockHeader* NextBlock(BlockHeader* p);

		PageHeader* m_pageList;

		BlockHeader* m_freeList;

		size_t m_dataSize;
		size_t m_pageSize;
		size_t m_alignmentSize;
		size_t m_blockSize;
		uint32_t m_blockPerPage;

		uint32_t m_numPages;
		uint32_t m_numBlocks;
		uint32_t m_numFreeBlocks;

		Allocator(const Allocator &clone);
		Allocator &operator=(const Allocator &rhs);
	};
}