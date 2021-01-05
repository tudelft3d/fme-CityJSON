/*=============================================================================

   Name     : geometryvisitor.cpp

   System   : FME Plug-in SDK

   Language : C++

   Purpose  : Declaration of FMECityJSONGeometryVisitor

         Copyright (c) 1994 - 2018, Safe Software Inc. All rights reserved.

   Redistribution and use of this sample code in source and binary forms, with 
   or without modification, are permitted provided that the following 
   conditions are met:
   * Redistributions of source code must retain the above copyright notice, 
     this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright notice, 
     this list of conditions and the following disclaimer in the documentation 
     and/or other materials provided with the distribution.

   THIS SAMPLE CODE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SAMPLE CODE, EVEN IF 
   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#include "fmecityjsongeometryvisitor.h"
#include "fmecityjsonpriv.h"
#include "fmecityjsonwriter.h"

#include <fmetypes.h>
#include <iaggregate.h>
#include <iarc.h>
#include <iorientedarc.h>
#include <iclothoid.h>
#include <iarea.h>
#include <iareaiterator.h>
#include <ibox.h>
#include <ibrepsolid.h>
#include <icompositesolid.h>
#include <icompositesurface.h>
#include <icsgsolid.h>
#include <icurveiterator.h>
#include <idonut.h>
#include <iellipse.h>
#include <iextrusion.h>
#include <iface.h>
#include <igeometryiterator.h>
#include <iline.h>
#include <ilogfile.h>
#include <imesh.h>
#include <imultiarea.h>
#include <imulticurve.h>
#include <imultipoint.h>
#include <imultisolid.h>
#include <imultisurface.h>
#include <imultitext.h>
#include <ipath.h>
#include <ipoint.h>
#include <ipointiterator.h>
#include <ipolygon.h>
#include <iraster.h>
#include <irectangleface.h>
#include <isegmentiterator.h>
#include <isession.h>
#include <isimpleareaiterator.h>
#include <isimplesoliditerator.h>
#include <isoliditerator.h>
#include <isurfaceiterator.h>
#include <itext.h>
#include <itextiterator.h>
#include <itrianglefan.h>
#include <itrianglestrip.h>

#include <string>

const std::map< std::string, std::vector< std::string > > FMECityJSONGeometryVisitor::semancticsTypes_ = std::map< std::string, std::vector< std::string > >(
   {
      {"Building", {
         "RoofSurface",
         "GroundSurface",
         "WallSurface",
         "ClosureSurface",
         "OuterCeilingSurface",
         "OuterFloorSurface",
         "Window",
         "Door"}},
      {"BuildingPart", {
         "RoofSurface",
         "GroundSurface",
         "WallSurface",
         "ClosureSurface",
         "OuterCeilingSurface",
         "OuterFloorSurface",
         "Window",
         "Door"}},
      {"BuildingInstallation", {
         "RoofSurface",
         "GroundSurface",
         "WallSurface",
         "ClosureSurface",
         "OuterCeilingSurface",
         "OuterFloorSurface",
         "Window",
         "Door"}},
      {"Railway", {
         "TrafficArea",
         "AuxiliaryTrafficArea"
      }},
      {"Road", {
         "TrafficArea",
         "AuxiliaryTrafficArea"
      }},
      {"TransportationSquare", {
         "TrafficArea",
         "AuxiliaryTrafficArea"
      }},
      {"WaterBody", {
         "WaterSurface",
         "WaterGroundSurface",
         "WaterClosureSurface"
      }}
   }
);

//===========================================================================
// Constructor.
FMECityJSONGeometryVisitor::FMECityJSONGeometryVisitor(const IFMEGeometryTools* geomTools,
                                                       IFMESession* session,
                                                       bool remove_duplicates,
                                                       int important_digits)
   :
   fmeGeometryTools_(geomTools),
   fmeSession_(session),
   remove_duplicates_(remove_duplicates),
   important_digits_(important_digits),
   skipLastPointOnLine_(false)
{
   logFile_ = session->logFile();
}

//===========================================================================
// Destructor.
FMECityJSONGeometryVisitor::~FMECityJSONGeometryVisitor()
{
   //------------------------------------------------------------------------
   // Perform any necessary cleanup
   //------------------------------------------------------------------------
}

json FMECityJSONGeometryVisitor::getGeomJSON()
{
   json retVal = outputgeom_;
   outputgeom_.clear();
   return retVal;
}

json FMECityJSONGeometryVisitor::takeWorkingBoundary()
{
   json retVal = workingBoundary_;
   workingBoundary_.clear();
   return retVal;
}

const VertexPool& FMECityJSONGeometryVisitor::getGeomVertices()
{
   return vertices_;
}

void FMECityJSONGeometryVisitor::getGeomBounds(std::optional<double>& minx,
                                               std::optional<double>& miny,
                                               std::optional<double>& minz,
                                               std::optional<double>& maxx,
                                               std::optional<double>& maxy,
                                               std::optional<double>& maxz)
{
   minx = minx_;
   miny = miny_;
   minz = minz_;
   maxx = maxx_;
   maxy = maxy_;
   maxz = maxz_;
}

bool FMECityJSONGeometryVisitor::semanticTypeAllowed(std::string trait)
{
   if (trait.compare(0, 1, "+") == 0) {
      return true;
   }
   auto iter = semancticsTypes_.find(featureType_);
   if (iter != semancticsTypes_.end())
   {
      std::vector<std::string> traits = iter->second;
      for (int i = 0; i < traits.size(); i++)
      {
         // logDebugMessage("comparing traits[i]: " + traits[i] + " & trait: " + trait);
         if (traits[i] == trait)
         {
            return true;
         }
      }
   }
   std::string message("CityJSON Semantic Surface Type '");
   message += featureType_;
   message += "' is not one of the CityJSON types(https://www.cityjson.org/specs/#semantic-surface-object) or an Extension ('+').";
   logFile_->logMessageString(message.c_str(), FME_WARN);
   return false;
}

void FMECityJSONGeometryVisitor::setFeatureType(std::string type) {
  featureType_ = type;
}

json FMECityJSONGeometryVisitor::replaceSemanticValues(std::vector<json> semanticValues) {
   // replace array with only null values with a single null value
   for (int i = 0; i < semanticValues.size(); i++) {
      if (!semanticValues[i].is_null()) {
         return semanticValues;
      }
   }
   return nullptr;
}

json FMECityJSONGeometryVisitor::replaceEmptySurface(std::vector<json> semanticSurface) {
   // replace array with only null values with a single null value
   if (semanticSurface.size() > 0) {
      return semanticSurface;
   }
   return std::vector<json>{nullptr};
}

void FMECityJSONGeometryVisitor::reset()
{
   outputgeom_.clear();
   surfaces_.clear();
   semanticValues_.clear();
}

// Converts a point into a string
std::string get_key(const FMECoord3D& vertex, int precision)
{
   char* buf = new char[200];
   std::stringstream ss;
   ss << "%." << precision << "f "
      << "%." << precision << "f "
      << "%." << precision << "f";
   std::sprintf(buf, ss.str().c_str(), vertex.x, vertex.y, vertex.z);
   std::string r(buf);
   return r;
}

void tokenize(const std::string& str, std::vector<std::string>& tokens)
{
   std::string::size_type lastPos = str.find_first_not_of(" ", 0);
   std::string::size_type pos     = str.find_first_of(" ", lastPos);
   while (std::string::npos != pos || std::string::npos != lastPos)
   {
      tokens.push_back(str.substr(lastPos, pos - lastPos));
      lastPos = str.find_first_not_of(" ", pos);
      pos     = str.find_first_of(" ", lastPos);
   }
}

void FMECityJSONGeometryVisitor::acceptVertex(const std::string& vertex_string)
{
   // Sadly, we need to calculate our bounds from the stringified vertex we
   // are using.  So back to float!
   std::vector<std::string> ls;
   tokenize(vertex_string, ls);
   double x = std::stod(ls[0]);
   double y = std::stod(ls[1]);
   double z = std::stod(ls[2]);

   if (!minx_ || x < minx_) minx_ = x;
   if (!maxx_ || x > maxx_) maxx_ = x;
   if (!miny_ || y < miny_) miny_ = y;
   if (!maxy_ || y > maxy_) maxy_ = y;
   if (!std::isnan(z)) // have z
   {
      if (!minz_ || z < minz_) minz_ = z;
      if (!maxz_ || z > maxz_) maxz_ = z;
   }

   vertices_.push_back({x, y, z});
}

// This will make sure we don't add any vertex twice.
unsigned long FMECityJSONGeometryVisitor::addVertex(const FMECoord3D& vertex)
{
   // This is the vertex, as a string, which we'll use.
   std::string vertex_string = get_key(vertex, important_digits_);

   // This will be the index in the vertex pool if it is new
   unsigned long index(vertices_.size());

   // A little more bookkeeping if we want to optimize the vertex pool
   // and not have duplicates.
   if (remove_duplicates_)
   {
      // Have we encountered this vertex before?
      auto [entry, vertexAdded] = vertexToIndex_.try_emplace(vertex_string, index);
      if (!vertexAdded) // We already have this in our vertex pool
      {
         index = entry->second;
      }
      else // We haven't seen this before, so insert it into the pool.
      {
         acceptVertex(vertex_string);

         // index is already set correctly.
      }
   }
   else
   {
      acceptVertex(vertex_string);

      // index is already set correctly.
   }
   return index;
}



//=====================================================================
//
bool FMECityJSONGeometryVisitor::claimTopLevel(const std::string& type)
{
   if (outputgeom_.empty())
   {
      outputgeom_ = json::object();
      outputgeom_["type"] = type;
      return true;
   }
   else
   {
      return false;

   }
}

//=====================================================================
//
void FMECityJSONGeometryVisitor::completedGeometry(bool topLevel, const json& boundary)
{
   if (topLevel)
   {
      outputgeom_["boundaries"] = boundary;
   }
   else
   {
      workingBoundary_ = boundary;
   }
}


//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitAggregate(const IFMEAggregate& aggregate)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("aggregate"));

   FME_Status badNews;

   // Create iterator to get all geometries
   //-- We can't write this out as an aggregate in CityJSON, so
   //   rather than fail, let's do the best we can - which is write
   //   them out as if they came in separately.
   IFMEGeometryIterator* iterator = aggregate.getIterator();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("aggregate geometry"));

      // re-visit aggregate geometries
      if (nullptr != iterator->getPart())
      {
         badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
         if (badNews)
         {
            // Destroy the iterator
            aggregate.destroyIterator(iterator);
            return FME_FAILURE;
         }
      }
   }
   // Done with the iterator, destroy it
   aggregate.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("aggregate"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitPoint(const IFMEPoint& point)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("point"));

   bool topLevel = claimTopLevel("MultiPoint");

   unsigned long index = addVertex({point.getX(), point.getY(), point.getZ()});

   // We have to make an array of only one!
   auto jsonArray = json::array();
   jsonArray.push_back(index);

   completedGeometry(topLevel, jsonArray);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("point"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiPoint(const IFMEMultiPoint& multipoint)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi point"));

   bool topLevel = claimTopLevel("MultiPoint");

   // Create iterator to get all point geometries
   IFMEPointIterator* iterator = multipoint.getIterator();
   auto jsonArray = json::array();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("point"));

      // re-visit points
      FME_Status badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator
         multipoint.destroyIterator(iterator);
         return FME_FAILURE;
      }
      jsonArray.push_back(takeWorkingBoundary());
   }
   // We are done with the iterator, so destroy it
   multipoint.destroyIterator(iterator);

   completedGeometry(topLevel, jsonArray);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi point"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitArc(const IFMEArc& arc)
{
   IFMELine* asLine  = arc.getAsLine();
   FME_Status result = visitLine(*asLine);
   fmeGeometryTools_->destroyGeometry(asLine);
   return result;
}

//-----------------------------------------------------------------------------
FME_Status FMECityJSONGeometryVisitor::visitOrientedArc(const IFMEOrientedArc & orientedArc)
{
   IFMELine* asLine = orientedArc.getAsLine();
   FME_Status result = visitLine(*asLine);
   fmeGeometryTools_->destroyGeometry(asLine);
   return result;
}

//-----------------------------------------------------------------------------
FME_Status FMECityJSONGeometryVisitor::visitClothoid(const IFMEClothoid& clothoid)
{
   IFMELine* asLine = clothoid.getAsLine();
   FME_Status result = visitLine(*asLine);
   fmeGeometryTools_->destroyGeometry(asLine);
   return result;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitLine(const IFMELine& line)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("line"));

   bool topLevel = claimTopLevel("MultiLineString");

   // Do we need to skip the last point?
   int skip = skipLastPointOnLine_ ? 1 : 0;

   auto jsonArray = json::array();
   for (int i = 0; i < line.numPoints()-skip; i++)
   {
      FMECoord3D point;
      line.getPointAt3D(i, point);
      unsigned long index = addVertex(point);
      jsonArray.push_back(index);
   }

   // If we are at the top level, just passed in a Line, we must convert this into
   // a MultiLineString, as CityJSON cannot store lines by themselves.
   if (topLevel)
   {
      auto jsonArray2 = json::array();
      jsonArray2.push_back(jsonArray);
      jsonArray = jsonArray2;
   }
   completedGeometry(topLevel, jsonArray);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("line"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitPath(const IFMEPath& path)
{
   IFMELine* asLine  = path.getAsLine();
   FME_Status result = visitLine(*asLine);
   fmeGeometryTools_->destroyGeometry(asLine);
   return result;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiCurve(const IFMEMultiCurve& multicurve)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi curve"));

   bool topLevel = claimTopLevel("MultiLineString");

   // Create an iterator to get the curves
   IFMECurveIterator* iterator = multicurve.getIterator();
   auto jsonArray = json::array();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("curve"));

      // re-visit the next curve
      FME_Status badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy the iterator and return fail
         multicurve.destroyIterator(iterator);
         return FME_FAILURE;
      }
      jsonArray.push_back(takeWorkingBoundary());
   }
   // Done visiting curves, destroy iterator
   multicurve.destroyIterator(iterator);

   completedGeometry(topLevel, jsonArray);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi curve"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiArea(const IFMEMultiArea& multiarea)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi area"));

   bool topLevel = claimTopLevel("MultiLineString");

   // Create iterator to visit all areas
   IFMEAreaIterator* iterator = multiarea.getIterator();
   auto jsonArray = json::array();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("area"));

      // re-visit areas
      FME_Status badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator
         multiarea.destroyIterator(iterator);
         return FME_FAILURE;
      }
      jsonArray.push_back(takeWorkingBoundary());
   }
   // Done with iterator, destroy it
   multiarea.destroyIterator(iterator);

   completedGeometry(topLevel, jsonArray);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi area"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitPolygon(const IFMEPolygon& polygon)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("polygon"));

   const IFMECurve* boundary = polygon.getBoundaryAsCurve();
   if (boundary == nullptr)
   {
      // We need a boundary, return fail
      return FME_FAILURE;
   }
   // re-visit polygon curve geometry
   FME_Status badNews = boundary->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitDonut(const IFMEDonut& donut)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("donut"));

   // Get the outer boundary
   logDebugMessage(std::string(kMsgVisiting) + std::string("outer boundary"));

   bool topLevel = claimTopLevel("MultiLineString");

   const IFMEArea* outerBoundary = donut.getOuterBoundaryAsSimpleArea();
   if (outerBoundary == nullptr)
   {
      // We require an outer boundary, return fail
      return FME_FAILURE;
   }
   // re-visit the outer boundary
   auto jsonArray = json::array();
   FME_Status badNews = outerBoundary->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }
   jsonArray.push_back(takeWorkingBoundary());

   // Get the inner boundary
   logDebugMessage(std::string(kMsgVisiting) + std::string("inner boundary"));

   IFMESimpleAreaIterator* iterator = donut.getIterator();
   while (iterator->next())
   {
      // re-visit the inner boundary
      badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         donut.destroyIterator(iterator);
         return FME_FAILURE;
      }
      jsonArray.push_back(takeWorkingBoundary());
   }
   // Done with iterator, destroy it
   donut.destroyIterator(iterator);

   completedGeometry(topLevel, jsonArray);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("donut"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitText(const IFMEText& text)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("text"));

   // This IFMEGeometryVisitorConst subclass does not consume the geometry which accepts it
   IFMEPoint* point = text.getLocationAsPoint();
   // re-visit location
   FME_Status badNews = visitPoint(*point);
   point->destroy(); point = nullptr;

   if (badNews)
   {
      return FME_FAILURE;
   }
   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiText(const IFMEMultiText& multitext)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi text"));

   bool topLevel = claimTopLevel("MultiPoint");

   // Create iterator to get all text geometries
   IFMETextIterator* iterator = multitext.getIterator();
   auto jsonArray = json::array();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("text"));

      // re-visit next text geometry
      FME_Status badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         multitext.destroyIterator(iterator);
         return FME_FAILURE;
      }
      jsonArray.push_back(takeWorkingBoundary());
   }
   // Done with iterator, destroy it
   multitext.destroyIterator(iterator);

   completedGeometry(topLevel, jsonArray);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi text"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitEllipse(const IFMEEllipse& ellipse)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("ellipse"));

   const IFMEArc* boundary = ellipse.getBoundaryAsArc();
   if (boundary == nullptr)
   {
      // We require a boundary
      return FME_FAILURE;
   }
   // re-visit points of the ellipse
   FME_Status badNews = boundary->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("ellipse"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitFace(const IFMEFace& face)
{
   skipLastPointOnLine_ = true;

   logDebugMessage(std::string(kMsgVisiting) + std::string("Face"));

   const IFMEArea* area = face.getAsArea();
   if (area == nullptr)
   {
      // We require an area
      return FME_FAILURE;
   }
   
   bool topLevel = claimTopLevel("CompositeSurface");

   // re-visit the boundary
   auto jsonArray = json::array();
   FME_Status badNews = area->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }
   jsonArray.push_back(takeWorkingBoundary());

   // If we are at the top level, just passed in a Face, we must convert this into 
   // a CompositeSurface, as CityJSON cannot store faces by themselves.
   if (topLevel)
   {
      auto jsonArray2 = json::array();
      jsonArray2.push_back(jsonArray);
      jsonArray = jsonArray2;
   }
   completedGeometry(topLevel, jsonArray);

   //-- fetch the semantic surface type of the geometry
   // Check if the semantics type is allowed
   // this needs the cityjson type to be known, pass cityjson type to constructor
   // and allow string to start with a '+'
   if (face.hasName()) {
      IFMEString* type = fmeSession_->createString();
      face.getName(*type, nullptr);

      if (semanticTypeAllowed(type->data())) {
         json surfaceSemantics;
         surfaceSemantics["type"] = type->data();
         fmeSession_->destroyString(type);

         //-- fetch the semantic surface traits of the geometry
         IFMEStringArray* traitNames = fmeSession_->createStringArray();
         face.getTraitNames(*traitNames);
         logDebugMessage(std::to_string(traitNames->entries()));
         for (int i = 0; i < traitNames->entries(); i++) {
            std::string traitNameStr = traitNames->elementAt(i)->data();

            // filter cityjson specific traits
            if (traitNameStr.compare(0, 9, "cityjson_") != 0) {
               FME_AttributeType type = face.getTraitType(*traitNames->elementAt(i));
               logDebugMessage("Found traitName with value: " + traitNameStr + " and type: " + std::to_string(type));

               if (type == FME_ATTR_STRING) {
                  IFMEString *geometryTrait = fmeSession_->createString();
                  face.getTraitString(*traitNames->elementAt(i), *geometryTrait);
                  surfaceSemantics[traitNameStr] = geometryTrait->data();
                  fmeSession_->destroyString(geometryTrait);
               }
               else if (type == FME_ATTR_REAL64) {
                  FME_Real64 geometryTrait;
                  face.getTraitReal64(*traitNames->elementAt(i), geometryTrait);
                  surfaceSemantics[traitNameStr] = geometryTrait;
               }
               else if (type == FME_ATTR_INT64) {
                  FME_Int64 geometryTrait;
                  face.getTraitInt64(*traitNames->elementAt(i), geometryTrait);
                  surfaceSemantics[traitNameStr] = geometryTrait;
               }
               else if (type == FME_ATTR_BOOLEAN) {
                  FME_Boolean geometryTrait;
                  face.getTraitBoolean(*traitNames->elementAt(i), geometryTrait);
                  if (geometryTrait == FME_FALSE) {
                     surfaceSemantics[traitNameStr] = false;
                  }
                  else {
                     surfaceSemantics[traitNameStr] = true;
                  }
               }
               else {
                  logFile_->logMessageString(
                     ("Semantic Surface attribute type '" + std::to_string(type) + "' is not allowed.").c_str(),
                     FME_WARN);
               }
            }
         }

         //-- De-duplicate surface semantics and keep correct number of semantic to store in values
         //-- Take into account not only semantics type since type can be same with different attribute values
         //-- Can use == to compare two json objects, comparison works on nested values of objects
         //-- check if semantic surface description exists
         int surfaceIdx = -1;
         for (int i = 0; i < surfaces_.size(); i++) {
            if (surfaces_[i] == surfaceSemantics) {
               surfaceIdx = i;
            }
         }
         if (surfaceIdx != -1) { // store value for existing semantic surface
            semanticValues_.push_back(surfaceIdx);
         }
         else { // store new semantic surface and its value 
            surfaces_.push_back(surfaceSemantics);
            semanticValues_.push_back(surfaces_.size() - 1);
         }
         fmeSession_->destroyStringArray(traitNames);
      }
      else {
         semanticValues_.push_back(nullptr);
      }
   }
   else {
      semanticValues_.push_back(nullptr);
   }

   skipLastPointOnLine_ = false;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitTriangleStrip(const IFMETriangleStrip& triangleStrip)
{
   skipLastPointOnLine_ = true;

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("triangle strip"));

   // This is kind of taking a shortcut.  Many formats do not support triangle fan, so they convert
   // it first. Convert the IFMETriangleStrip to a mesh, then to IFMECompositeSurface
   IFMEMesh* mesh = fmeGeometryTools_->createTriangulatedMeshFromGeometry(triangleStrip);
   IFMECompositeSurface* geomCompositeSurface = mesh->getAsCompositeSurface();

   logDebugMessage(std::string(kMsgVisiting) + std::string("triangle strip as composite surface"));

   // re-visit the solid geometry
   FME_Status badNews = visitCompositeSurface(*geomCompositeSurface);
   // Done with the geomCompositeSurface and mesh
   geomCompositeSurface->destroy(); geomCompositeSurface = nullptr;
   mesh->destroy(); mesh = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("triangle strip"));

   skipLastPointOnLine_ = false;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitTriangleFan(const IFMETriangleFan& triangleFan)
{
   skipLastPointOnLine_ = true; 

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("triangle fan"));

   // This is kind of taking a shortcut.  Many formats do not support triangle fan, so they convert it
   // first. Convert the IFMETriangleFan to a mesh, then to IFMECompositeSurface
   IFMEMesh* mesh = fmeGeometryTools_->createTriangulatedMeshFromGeometry(triangleFan);
   IFMECompositeSurface* geomCompositeSurface = mesh->getAsCompositeSurface();

   logDebugMessage(std::string(kMsgVisiting) + std::string("triangle fan as composite surface"));

   // re-visit the solid geometry
   FME_Status badNews = visitCompositeSurface(*geomCompositeSurface);
   // Done with the geomCompositeSurface and mesh
   geomCompositeSurface->destroy(); geomCompositeSurface = nullptr;
   mesh->destroy(); mesh = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("triangle fan"));

   skipLastPointOnLine_ = false;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitBox(const IFMEBox& box)
{
   skipLastPointOnLine_ = true; 

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("box"));

   // This is kind of taking a shortcut.  Many formats do not support Box, so they convert it
   // first. Convert the IFMEBox to a IFMEBRepSolid
   const IFMEBRepSolid* brepSolid = box.getAsBRepSolid(); // we don't own this.

   logDebugMessage(std::string(kMsgVisiting) + std::string("box as brep solid"));

   // re-visit the solid geometry
   FME_Status badNews = visitBRepSolid(*brepSolid);
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("box"));

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitExtrusion(const IFMEExtrusion& extrusion)
{
   skipLastPointOnLine_ = true; 

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("extrusion"));

   // This is kind of taking a shortcut.  Many formats do not support Extrusion, so they convert it
   // first. Convert the IFMEExtrusion to a IFMEBRepSolid
   const IFMEBRepSolid* brepSolid = extrusion.getAsBRepSolid(); // we don't own this.

   logDebugMessage(std::string(kMsgVisiting) + std::string("extrusion as brep solid"));

   // re-visit the solid geometry
   FME_Status badNews = visitBRepSolid(*brepSolid);
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("extrusion"));

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitBRepSolid(const IFMEBRepSolid& brepSolid)
{
   skipLastPointOnLine_ = true; 

   solidSemanticValues_.clear();

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("Solid"));

   bool topLevel = claimTopLevel("Solid");

   // Get the outer surface
   logDebugMessage(std::string(kMsgVisiting) + std::string("outer surface"));
   const IFMESurface* outerSurface = brepSolid.getOuterSurface();
   if (outerSurface == nullptr)
   {
      // Need an outer surface
      return FME_FAILURE;
   }

   // re-visit the outer surface geometry
   FME_Status badNews = outerSurface->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }
   auto jsonArray = json::array();
   jsonArray.push_back(takeWorkingBoundary());
   solidSemanticValues_.push_back(replaceSemanticValues(semanticValues_));


   // Create iterator to loop though all the inner surfaces
   IFMESurfaceIterator* iterator = brepSolid.getIterator();
   while (iterator->next())
   {
      // Get the next inner surface
      const IFMESurface* innerSurface = iterator->getPart();
      logDebugMessage(std::string(kMsgVisiting) + std::string("inner surface"));
      // re-visit the inner surface geometry
      badNews = innerSurface->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator before leaving
         brepSolid.destroyIterator(iterator);
         return FME_FAILURE;
      }
      jsonArray.push_back(takeWorkingBoundary());
      solidSemanticValues_.push_back(replaceSemanticValues(semanticValues_));
   }

   completedGeometry(topLevel, jsonArray);

   //-- store semantic surface information
   if (!semanticValues_.empty()) {
     outputgeom_["semantics"]["surfaces"] = replaceEmptySurface(surfaces_);
     outputgeom_["semantics"]["values"] = solidSemanticValues_;
   }

   // Done with the iterator
   brepSolid.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("boundary representation solid"));

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

FME_Status FMECityJSONGeometryVisitor::visitCompositeSurfaceParts(
   const IFMECompositeSurface& compositeSurface, json& jsonArray)
{
   skipLastPointOnLine_ = true; 

   IFMESurfaceIterator* iterator = compositeSurface.getIterator();
   while (iterator->next())
   {
      // Get the next surface
      const IFMESurface* surface = iterator->getPart();

      // Can't deal with multiple levels of nesting, so let's break that down.
      if (surface->canCastAs<const IFMECompositeSurface*>())
      {
         FME_Status badNews = visitCompositeSurfaceParts(*(surface->castAs<const IFMECompositeSurface*>()), jsonArray);
         if (badNews) return FME_FAILURE;
      }
      else
      {
         logDebugMessage(std::string(kMsgVisiting) + std::string("surface"));

         // re-visit the surface geometry
         FME_Status badNews = surface->acceptGeometryVisitorConst(*this);
         if (badNews)
         {
            // Destroy iterator before leaving
            compositeSurface.destroyIterator(iterator);
            return FME_FAILURE;
         }
         jsonArray.push_back(takeWorkingBoundary());
      }
   }

   // Done with the iterator
   compositeSurface.destroyIterator(iterator);

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCompositeSurface(const IFMECompositeSurface& compositeSurface)
{
   skipLastPointOnLine_ = true; 

   semanticValues_.clear();

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("CompositeSurface"));

   bool topLevel = claimTopLevel("CompositeSurface");

   // Create an iterator to loop through all the surfaces this multi surface contains
   auto jsonArray = json::array();
   FME_Status badNews = visitCompositeSurfaceParts(compositeSurface, jsonArray);
   if (badNews) return FME_FAILURE;

   completedGeometry(topLevel, jsonArray);

   //-- store semantic surface information
   if (!semanticValues_.empty()) {
      outputgeom_["semantics"]["surfaces"] = surfaces_;
      outputgeom_["semantics"]["values"] = semanticValues_;
   }

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitRectangleFace(const IFMERectangleFace& rectangle)
{
   skipLastPointOnLine_ = true; 

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("rectangle face"));

   // This is kind of taking a shortcut.  Many formats do not support rectangle face, so they convert it
   // first. Convert the IFMERectangleFace to a IFMEFace
   IFMEFace* face = rectangle.getAsFaceCopy();

   logDebugMessage(std::string(kMsgVisiting) + std::string("rectangle face as face"));

   // re-visit the solid geometry
   FME_Status badNews = visitFace(*face);
   // Done with the face
   face->destroy(); face = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("rectangle face"));

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiSurface(const IFMEMultiSurface& multiSurface)
{
   skipLastPointOnLine_ = true; 

   semanticValues_.clear();

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi surface"));
      
   bool topLevel = claimTopLevel("MultiSurface");

   // Create an iterator to loop through all the surfaces this multi surface contains
   IFMESurfaceIterator* iterator = multiSurface.getIterator();
   auto jsonArray = json::array();
   while (iterator->next())
   {
      // Get the next surface
      const IFMESurface* surface = iterator->getPart();

      logDebugMessage(std::string(kMsgVisiting) + std::string("surface"));

      // re-visit the surface geometry
      FME_Status badNews = surface->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator before leaving
         multiSurface.destroyIterator(iterator);
         return FME_FAILURE;
      }
      jsonArray.push_back(takeWorkingBoundary());
   }

   completedGeometry(topLevel, jsonArray);

   //-- store semantic surface information
   if (!semanticValues_.empty()) {
     outputgeom_["semantics"]["surfaces"] = surfaces_;
     outputgeom_["semantics"]["values"] = semanticValues_;
   }

   // Done with the iterator
   multiSurface.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi surface"));

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiSolid(const IFMEMultiSolid& multiSolid)
{
   // Use some shared code.
   return visitCompositeOrMultiSolid(multiSolid, "MultiSolid");
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCompositeSolid(const IFMECompositeSolid& compositeSolid)
{
   // Use some shared code.
   return visitCompositeOrMultiSolid(compositeSolid, "CompositeSolid");
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCSGSolid(const IFMECSGSolid& csgSolid)
{
   skipLastPointOnLine_ = true; 

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("CSG solid"));

   // This is kind of taking a shortcut.  Many formats do not support CSGSolid, so they convert it first.
   // Convert the IFMECSGSolid to either an IFMEMultiSolid, IFMEBRepSolid, or IFMENull.
   const IFMEGeometry* geomCSGSolid = csgSolid.evaluateCSG();

   logDebugMessage(std::string(kMsgVisiting) + std::string("CSG solid component"));

   // re-visit the solid geometry
   FME_Status badNews = geomCSGSolid->acceptGeometryVisitorConst(*this);
   // Done with the geomCSGSolid
   geomCSGSolid->destroy(); geomCSGSolid = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("CSG solid"));
   
   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMesh( const IFMEMesh& mesh )
{
   skipLastPointOnLine_ = true; 

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("mesh"));

   // This is kind of taking a shortcut.  Many formats do not support Mesh, so they convert it
   // first. Convert the IFMEMesh to either an IFMECompositeSurface.
   IFMECompositeSurface* geomCompositeSurface = mesh.getAsCompositeSurface();

   logDebugMessage(std::string(kMsgVisiting) + std::string("mesh as composite surface"));

   // re-visit the composite surface geometry
   FME_Status badNews = visitCompositeSurface(*geomCompositeSurface);
   // Done with the geomCompositeSurface
   geomCompositeSurface->destroy(); geomCompositeSurface = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("mesh"));

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitNull(const IFMENull& fmeNull)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("null"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitRaster(const IFMERaster& raster)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("raster"));

   return FME_SUCCESS;
}

//=====================================================================
FME_Status FMECityJSONGeometryVisitor::visitPointCloud(const IFMEPointCloud& pointCloud) 
{
   //TODO: PR:26877 This method should be written.
   logDebugMessage(std::string(kMsgVisiting) + std::string("pointCloud"));
   return FME_SUCCESS;
}

//=====================================================================
FME_Status FMECityJSONGeometryVisitor::visitFeatureTable(const IFMEFeatureTable& featureTable)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("featureTable"));
   return FME_SUCCESS;
}

//=====================================================================
FME_Status FMECityJSONGeometryVisitor::visitVoxelGrid(const IFMEVoxelGrid& /*voxelGrid*/)
{
   //TODO: [FMEENGINE-63998] This method should be written.
   logDebugMessage(std::string(kMsgVisiting) + std::string("voxel"));
   return FME_SUCCESS;
}


