/*! \file */ 
#include "tile_worker.h"
#include <fstream>
#include <boost/filesystem.hpp>
#include "helpers.h"
#include "write_geometry.h"
using namespace std;
extern bool verbose;

typedef std::pair<double,double> xy_pair;

// Connect disconnected linestrings within a MultiLinestring
void ReorderMultiLinestring(MultiLinestring &input, MultiLinestring &output) {
	// create a map of the start points of each linestring
	// (we should be able to do std::map<Point,unsigned>, but that errors)
	std::map<xy_pair,unsigned> startPoints;
	for (unsigned i=0; i<input.size(); i++) {
		startPoints[xy_pair(input[i][0].x(),input[i][0].y())] = i;
	}

	// then for each linestring:
	// [skip if it's already been handled]
	// 1. create an output linestring from it
	// 2. look to see if there's another linestring which starts at the end place
	// 3. if there is, then append it, remove from the map, and repeat from 2
	std::vector<bool> added(input.size(), false);
	for (unsigned i=0; i<input.size(); i++) {
		if (added[i]) continue;
		Linestring ls = std::move(input[i]);
		added[i] = true;
		while (true) {
			Point lastPoint = ls[ls.size()-1];
			auto foundStart = startPoints.find(xy_pair(lastPoint.x(),lastPoint.y()));
			if (foundStart == startPoints.end()) break;
			unsigned idx = foundStart->second;
			if (added[idx]) break;
			for (unsigned j=1; j<input[idx].size(); j++) ls.emplace_back(input[idx][j]);
			added[idx] = true;
		}
		output.resize(output.size()+1);
		output[output.size()-1] = std::move(ls);
	}
}

void CheckNextObjectAndMerge(OSMStore &osmStore, ObjectsAtSubLayerIterator &jt, const ObjectsAtSubLayerIterator &ooSameLayerEnd, 
	const TileBbox &bbox, Geometry &g) {

	// If a object is a polygon or a linestring that is followed by
	// other objects with the same geometry type and the same attributes,
	// the following objects are merged into the first object, by taking union of geometries.
	OutputObjectRef oo = *jt;
	OutputObjectRef ooNext;
	if(jt+1 != ooSameLayerEnd) ooNext = *(jt+1);

	auto gTyp = oo->geomType;

	if (gTyp == OutputGeometryType::LINESTRING) {
		MultiLinestring *gAcc = nullptr;
		try {
			gAcc = &boost::get<MultiLinestring>(g);
		} catch (boost::bad_get &err) {
			cerr << "Error: LineString " << oo->objectID << " has unexpected type" << endl;
			return;
		}

		while (jt+1 != ooSameLayerEnd &&
				ooNext->geomType == gTyp &&
				ooNext->attributes == oo->attributes) {
			jt++;
			oo = *jt;
			if(jt+1 != ooSameLayerEnd) ooNext = *(jt+1);
			else ooNext.reset();

			try {
				MultiLinestring gNew = boost::get<MultiLinestring>(buildWayGeometry(osmStore, *oo, bbox));
				MultiLinestring gTmp;
				geom::union_(*gAcc, gNew, gTmp);
				MultiLinestring reordered;
				ReorderMultiLinestring(gTmp, reordered);
				*gAcc = move(reordered);
			} catch (std::out_of_range &err) {
				if (verbose) cerr << "Error while processing LINESTRING " << oo->geomType << "," << oo->objectID <<"," << err.what() << endl;
			} catch (boost::bad_get &err) {
				cerr << "Error while processing LINESTRING " << oo->objectID << " has unexpected type" << endl;
				continue;
			}
		}
		
		
	}
}

void ProcessObjects(OSMStore &osmStore, const ObjectsAtSubLayerIterator &ooSameLayerBegin, const ObjectsAtSubLayerIterator &ooSameLayerEnd, 
	class SharedData &sharedData, double simplifyLevel, double filterArea, unsigned zoom, const TileBbox &bbox,
	vector_tile::Tile_Layer *vtLayer, vector<string> &keyList, vector<vector_tile::Tile_Value> &valueList) {

	for (ObjectsAtSubLayerIterator jt = ooSameLayerBegin; jt != ooSameLayerEnd; ++jt) {
		OutputObjectRef oo = *jt;
		if (zoom < oo->minZoom) { continue; }

		if (oo->geomType == OutputGeometryType::POINT) {
			vector_tile::Tile_Feature *featurePtr = vtLayer->add_features();
			LatpLon pos = buildNodeGeometry(osmStore, *oo, bbox);
			featurePtr->add_geometry(9);					// moveTo, repeat x1
			pair<int,int> xy = bbox.scaleLatpLon(pos.latp/10000000.0, pos.lon/10000000.0);
			featurePtr->add_geometry((xy.first  << 1) ^ (xy.first  >> 31));
			featurePtr->add_geometry((xy.second << 1) ^ (xy.second >> 31));
			featurePtr->set_type(vector_tile::Tile_GeomType_POINT);

			oo->writeAttributes(&keyList, &valueList, featurePtr, zoom);
			if (sharedData.config.includeID) { featurePtr->set_id(oo->objectID); }
		} else {
			Geometry g;
			try {
				g = buildWayGeometry(osmStore, *oo, bbox);
			} catch (std::out_of_range &err) {
				if (verbose) cerr << "Error while processing geometry " << oo->geomType << "," << oo->objectID <<"," << err.what() << endl;
				continue;
			}
			if (oo->geomType == OutputGeometryType::POLYGON && filterArea > 0.0) {
				if (geom::area(g)<filterArea) continue;
			}

			//This may increment the jt iterator
			if(zoom < sharedData.config.combineBelow) {
				CheckNextObjectAndMerge(osmStore, jt, ooSameLayerEnd, bbox, g);
				oo = *jt;
			}

			vector_tile::Tile_Feature *featurePtr = vtLayer->add_features();
			WriteGeometryVisitor w(&bbox, featurePtr, simplifyLevel);
			boost::apply_visitor(w, g);
			if (featurePtr->geometry_size()==0) { vtLayer->mutable_features()->RemoveLast(); continue; }
			oo->writeAttributes(&keyList, &valueList, featurePtr, zoom);
			if (sharedData.config.includeID) { featurePtr->set_id(oo->objectID); }

		}
	}
}

void ProcessLayer(OSMStore &osmStore,
    uint zoom, const TilesAtZoomIterator &it, vector_tile::Tile &tile, 
	const TileBbox &bbox, const std::vector<uint> &ltx, SharedData &sharedData)
{
	TileCoordinates index = it.GetCoordinates();

	vector<string> keyList;
	vector<vector_tile::Tile_Value> valueList;
	vector_tile::Tile_Layer *vtLayer = tile.add_layers();

	//TileCoordinate tileX = index.x;
	TileCoordinate tileY = index.y;

	// Loop through sub-layers
	for (auto mt = ltx.begin(); mt != ltx.end(); ++mt) {
		uint layerNum = *mt;
		const LayerDef &ld = sharedData.layers.layers[layerNum];
		if (zoom<ld.minzoom || zoom>ld.maxzoom) { continue; }
		double simplifyLevel = 0.0, filterArea = 0.0, latp = 0.0;
		if (zoom < ld.simplifyBelow || zoom < ld.filterBelow) {
			latp = (tiley2latp(tileY, zoom) + tiley2latp(tileY+1, zoom)) / 2;
		}
		if (zoom < ld.simplifyBelow) {
			if (ld.simplifyLength > 0) {
				simplifyLevel = meter2degp(ld.simplifyLength, latp);
			} else {
				simplifyLevel = ld.simplifyLevel;
			}
			simplifyLevel *= pow(ld.simplifyRatio, (ld.simplifyBelow-1) - zoom);
		}
		if (zoom < ld.filterBelow) { 
			filterArea = meter2degp(ld.filterArea, latp) * pow(2.0, (ld.filterBelow-1) - zoom);
		}

		ObjectsAtSubLayerConstItPair ooListSameLayer = it.GetObjectsAtSubLayer(layerNum);
		// Loop through output objects
		ProcessObjects(osmStore, ooListSameLayer.first, ooListSameLayer.second, sharedData, 
			simplifyLevel, filterArea, zoom, bbox, vtLayer, keyList, valueList);
	}

	// If there are any objects, then add tags
	if (vtLayer->features_size()>0) {
		vtLayer->set_name(sharedData.layers.layers[ltx.at(0)].name);
		vtLayer->set_version(sharedData.config.mvtVersion);
		vtLayer->set_extent(4096);
		for (uint j=0; j<keyList.size()  ; j++) {
			vtLayer->add_keys(keyList[j]);
		}
		for (uint j=0; j<valueList.size(); j++) { 
			vector_tile::Tile_Value *v = vtLayer->add_values();
			*v = valueList[j];
		}
	} else {
		tile.mutable_layers()->RemoveLast();
	}
}

bool outputProc(boost::asio::thread_pool &pool, SharedData &sharedData, OSMStore &osmStore, TilesAtZoomIterator const &it, uint zoom) {

	// Create tile
	vector_tile::Tile tile;
	TileBbox bbox(it.GetCoordinates(), zoom);
	if (sharedData.config.clippingBoxFromJSON && (sharedData.config.maxLon<=bbox.minLon 
		|| sharedData.config.minLon>=bbox.maxLon || sharedData.config.maxLat<=bbox.minLat 
		|| sharedData.config.minLat>=bbox.maxLat)) { return true; }

	// Loop through layers
	for (auto lt = sharedData.layers.layerOrder.begin(); lt != sharedData.layers.layerOrder.end(); ++lt) {
		ProcessLayer(osmStore, zoom, it, tile, bbox, *lt, sharedData);
	}

	// Write to file or sqlite
	string data, compressed;
	if (sharedData.sqlite) {
		// Write to sqlite
		tile.SerializeToString(&data);
		if (sharedData.config.compress) { compressed = compress_string(data, Z_DEFAULT_COMPRESSION, sharedData.config.gzip); }
		sharedData.mbtiles.saveTile(zoom, bbox.index.x, bbox.index.y, sharedData.config.compress ? &compressed : &data);

	} else {
		// Write to file
		stringstream dirname, filename;
		dirname  << sharedData.outputFile << "/" << zoom << "/" << bbox.index.x;
		filename << sharedData.outputFile << "/" << zoom << "/" << bbox.index.x << "/" << bbox.index.y << ".pbf";
		boost::filesystem::create_directories(dirname.str());
		fstream outfile(filename.str(), ios::out | ios::trunc | ios::binary);
		if (sharedData.config.compress) {
			tile.SerializeToString(&data);
			outfile << compress_string(data, Z_DEFAULT_COMPRESSION, sharedData.config.gzip);
		} else {
			if (!tile.SerializeToOstream(&outfile)) { cerr << "Couldn't write to " << filename.str() << endl; return false; }
		}
		outfile.close();
	}

	return true;
}

