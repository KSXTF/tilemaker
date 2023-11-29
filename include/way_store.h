#ifndef _WAY_STORE_H
#define _WAY_STORE_H

#include <utility>
#include "coordinates.h"
#include "mmap_allocator.h"

class WayStore {
public:
	using latplon_vector_t = std::vector<LatpLon, mmap_allocator<LatpLon>>;
	using ll_element_t = std::pair<WayID, latplon_vector_t>;

	virtual void reopen() = 0;
	virtual const latplon_vector_t& at(WayID wayid) const = 0;
	virtual bool requiresNodes() const = 0;
	virtual void insertLatpLons(std::vector<ll_element_t>& newWays) = 0;
	virtual const void insertNodes(const std::vector<std::pair<WayID, std::vector<NodeID>>>& newWays) = 0;
	virtual void clear() = 0;
	virtual std::size_t size() const = 0;
	virtual void finalize(unsigned int threadNum) = 0;
};

#endif
