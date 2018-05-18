#ifndef _OUTPUT_OBJECT_H
#define _OUTPUT_OBJECT_H

/*
	OutputObject - any object (node, linestring, polygon) to be outputted to tiles

	Possible future improvements to save memory:
	- use a global dictionary for attribute key/values
*/

#include <vector>
#include <string>
#include <map>
#include <iostream>
#include "geomtypes.h"
#include "coordinates.h"
#include "osm_store.h"

#include "clipper.hpp"

// Protobuf
#include "osmformat.pb.h"
#include "vector_tile.pb.h"

enum OutputGeometryType { POINT, LINESTRING, POLYGON, CENTROID, CACHED_POINT, CACHED_LINESTRING, CACHED_POLYGON };

class ClipGeometryVisitor : public boost::static_visitor<Geometry> {

	const Box &clippingBox; //for boost ggl
	ClipperLib::Path clippingPath; //for clipper library

public:
	ClipGeometryVisitor(const Box &cbox);

	Geometry operator()(const Point &p) const;

	Geometry operator()(const Linestring &ls) const;

	Geometry operator()(const MultiLinestring &mls) const;

	Geometry operator()(const MultiPolygon &mp) const;
};

class OutputObject { 

public:
	OutputGeometryType geomType;						// point, linestring, polygon...
	uint_least8_t layer;								// what layer is it in?
	NodeID objectID;									// id of way (linestring/polygon) or node (point)
	std::map <std::string, vector_tile::Tile_Value> attributes;	// attributes

	OutputObject(OutputGeometryType type, uint_least8_t l, NodeID id);
	
	void addAttribute(const std::string &key, vector_tile::Tile_Value &value);

	bool hasAttribute(const std::string &key) const;

	// Assemble a linestring or polygon into a Boost geometry, and clip to bounding box
	// Returns a boost::variant -
	//   POLYGON->MultiPolygon, CENTROID->Point, LINESTRING->MultiLinestring
	Geometry buildWayGeometry(const OSMStore &osmStore,
	                      TileBbox *bboxPtr, 
	                      const std::vector<Geometry> &cachedGeometries) const;
	
	// Add a node geometry
	void buildNodeGeometry(LatpLon ll, TileBbox *bboxPtr, vector_tile::Tile_Feature *featurePtr) const;
	
	// Write attribute key/value pairs (dictionary-encoded)
	void writeAttributes(std::vector<std::string> *keyList, std::vector<vector_tile::Tile_Value> *valueList, vector_tile::Tile_Feature *featurePtr) const;
	
	// Find a value in the value dictionary
	// (we can't easily use find() because of the different value-type encoding - 
	//  should be possible to improve this though)
	int findValue(std::vector<vector_tile::Tile_Value> *valueList, vector_tile::Tile_Value *value) const;
};

// Comparision functions

bool operator==(const OutputObject &x, const OutputObject &y);

// Do lexicographic comparison, with the order of: layer, geomType, attributes, and objectID.
// Note that attributes is preffered to objectID.
// It is to arrange objects with the identical attributes continuously.
// Such objects will be merged into one object, to reduce the size of output.
bool operator<(const OutputObject &x, const OutputObject &y);

namespace vector_tile {
	bool operator==(const vector_tile::Tile_Value &x, const vector_tile::Tile_Value &y);
	bool operator<(const vector_tile::Tile_Value &x, const vector_tile::Tile_Value &y);
}

// Hashing function so we can use an unordered_set

namespace std {
	template<>
	struct hash<OutputObject> {
		size_t operator()(const OutputObject &oo) const {
			return std::hash<uint_least8_t>()(oo.layer) ^ std::hash<NodeID>()(oo.objectID);
		}
	};
}

#endif //_OUTPUT_OBJECT_H
