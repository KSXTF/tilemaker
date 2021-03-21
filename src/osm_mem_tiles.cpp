#include "osm_mem_tiles.h"
using namespace std;

OsmMemTiles::OsmMemTiles(uint baseZoom):
	TileDataSource(),
	baseZoom(baseZoom) { }

void OsmMemTiles::MergeTileCoordsAtZoom(uint zoom, TileCoordinatesSet &dstCoords) {
	::MergeTileCoordsAtZoom(zoom, baseZoom, tileIndex, dstCoords);
}

void OsmMemTiles::MergeSingleTileDataAtZoom(TileCoordinates dstIndex, uint zoom, OutputObjectsPerLayer &dstTile) {
	::MergeSingleTileDataAtZoom(dstIndex, zoom, baseZoom, tileIndex, dstTile);
}

void OsmMemTiles::AddObject(TileCoordinates index, OutputObjectRef oo) {
	tileIndex[index].push_back(oo);
}

void OsmMemTiles::Clear() {
	tileIndex.clear();
}
