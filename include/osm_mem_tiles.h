/*! \file */ 
#ifndef _OSM_MEM_TILES
#define _OSM_MEM_TILES

#include "tile_data.h"
#include "osm_store.h"

/**
	\brief OsmMemTiles stores OSM objects in memory and provides a vector of OutputObjectRef for specified tiles
	
	The input objects are generated by PbfReader. The output objects are sent to OsmMemTiles for storage.
*/
class OsmMemTiles : public TileDataSource
{
public:
	OsmMemTiles(uint baseZoom);

	///This must be thread safe!
	virtual void MergeTileCoordsAtZoom(uint zoom, TileCoordinatesSet &dstCoords);

	///This must be thread safe!
	virtual void MergeSingleTileDataAtZoom(TileCoordinates dstIndex, uint zoom, 
		std::vector<OutputObjectRef> &dstTile);

	virtual void AddObject(TileCoordinates index, OutputObjectRef oo);

private:
	//This variables are generally safe for multiple threads to read, but not to write.

	TileIndex tileIndex;
	uint baseZoom;
};

#endif //_OSM_MEM_TILES

