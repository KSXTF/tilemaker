/*! \file */ 
#ifndef _OSM_LUA_PROCESSING_H
#define _OSM_LUA_PROCESSING_H

#include <vector>
#include <string>
#include <sstream>
#include <map>
#include "geomtypes.h"
#include "osm_store.h"
#include "shared_data.h"
#include "output_object.h"
#include "read_pbf.h"
#include "shp_mem_tiles.h"
#include "osm_mem_tiles.h"

// Lua
extern "C" {
	#include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}
#include "kaguya.hpp"

/**
	\brief OsmLuaProcessing - converts OSM objects into OutputObjectOsmStore objects.
	
	The input objects are generated by PbfReader. The output objects are sent to OsmMemTiles for storage.

	This class provides a consistent interface for Lua scripts to access.
*/
class OsmLuaProcessing : public PbfReaderOutput
{ 

public:
	// ----	initialization routines

	OsmLuaProcessing(const class Config &configIn, class LayerDefinition &layers, 
		const std::string &luaFile,
		const class ShpMemTiles &shpMemTiles, 
		class TileDataSource &outputTiles);
	virtual ~OsmLuaProcessing();

	// ----	Helpers provided for main routine

	// Has this object been assigned to any layers?
	bool empty();

	// ----	Data loading methods

	/// \brief Called when data loading is starting
	virtual void startOsmData();

	virtual void everyNode(NodeID id, LatpLon node);

	/// \brief We are now processing a significant node
	virtual void setNode(NodeID id, LatpLon node, const std::map<std::string, std::string> &tags);

	/// \brief We are now processing a way
	virtual void setWay(Way *way, NodeVec *nodeVecPtr, bool inRelation, const std::map<std::string, std::string> &tags);

	/** \brief We are now processing a relation
	 * (note that we store relations as ways with artificial IDs, and that
	 *  we use decrementing positive IDs to give a bit more space for way IDs)
	 */
	virtual void setRelation(Relation *relation, WayVec *outerWayVecPtr, WayVec *innerWayVecPtr,
		const std::map<std::string, std::string> &tags);

	/// \brief Called when data loading for a single file is done
	virtual void endOsmData();

	// ----	Metadata queries called from Lua

	// Get the ID of the current object
	std::string Id() const;

	// Check if there's a value for a given key
	bool Holds(const std::string& key) const;

	// Get an OSM tag for a given key (or return empty string if none)
	std::string Find(const std::string& key) const;

	// ----	Spatial queries called from Lua

	// Find intersecting shapefile layer
	std::vector<std::string> FindIntersecting(const std::string &layerName);
	bool Intersects(const std::string &layerName);

	// Returns whether it is closed polygon
	bool IsClosed() const;

	// Scale to (kilo)meter
	double ScaleToMeter();

	double ScaleToKiloMeter();

	// Returns area
	double Area();

	// Returns length
	double Length();

	// ----	Requests from Lua to write this way/node to a vector tile's Layer

	// Add layer
	void Layer(const std::string &layerName, bool area);
	void LayerAsCentroid(const std::string &layerName);
	
	// Set attributes in a vector tile's Attributes table
	void Attribute(const std::string &key, const std::string &val);
	void AttributeNumeric(const std::string &key, const float val);
	void AttributeBoolean(const std::string &key, const bool val);

	// ----	vector_layers metadata entry

	void setVectorLayerMetadata(const uint_least8_t layer, const std::string &key, const uint type);

	std::vector<std::string> GetSignificantNodeKeys();

	// ---- Cached geometries creation

	const Linestring &linestringCached();

	const Polygon &polygonCached();

	const MultiPolygon &multiPolygonCached();

private:
	/// Internal: clear current cached state
	inline void reset() {
		outputs.clear();
		linestringInited = false;
		polygonInited = false;
		multiPolygonInited = false;
	}

	// Internal: set start/end co-ordinates
	inline void setLocation(int32_t a, int32_t b, int32_t c, int32_t d) {
		lon1=a; latp1=b; lon2=c; latp2=d;
	}

	OSMStore osmStore;									// global OSM store

	kaguya::State luaState;
	const class ShpMemTiles &shpMemTiles;
	class TileDataSource &outputTiles;

	uint64_t osmID;							///< ID of OSM object
	WayID newWayID = MAX_WAY_ID;			///< Decrementing new ID for relations
	bool isWay, isRelation;					///< Way, node, relation?

	int32_t lon1,latp1,lon2,latp2;			///< Start/end co-ordinates of OSM object
	NodeVec *nodeVec;						///< node vector
	WayVec *outerWayVec, *innerWayVec;		///< way vectors

	Linestring linestringCache;
	bool linestringInited;
	Polygon polygonCache;
	bool polygonInited;
	MultiPolygon multiPolygonCache;
	bool multiPolygonInited;

	const class Config &config;
	class LayerDefinition &layers;
	
	std::vector<OutputObjectRef> outputs;			///< All output objects that have been created
	std::map<std::string, std::string> currentTags;

};

#endif //_OSM_LUA_PROCESSING_H
