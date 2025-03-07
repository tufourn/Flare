#pragma once

#include <glm/glm.hpp>
#include <mikktspace.h>
#include <vector>

namespace Flare {
struct CalcTangentData {
  const std::vector<uint32_t> &indices;
  const std::vector<glm::vec4> &positions;
  const std::vector<glm::vec4> &normals;
  const std::vector<glm::vec2> &uvs;
  std::vector<glm::vec4> &tangents;
};

struct CalcTangent {
  CalcTangent();

  void calculate(CalcTangentData *data);

  SMikkTSpaceInterface interface = {};
  SMikkTSpaceContext context = {};

  static int getVertexIndex(const SMikkTSpaceContext *pContext, int iFace,
                            int iVert);

  static int getNumFaces(const SMikkTSpaceContext *pContext);

  static int getNumVerticesOfFace(const SMikkTSpaceContext *pContext,
                                  const int iFace);

  static void getPosition(const SMikkTSpaceContext *pContext, float fvPosOut[],
                          const int iFace, const int iVert);

  static void getNormal(const SMikkTSpaceContext *pContext, float fvNormOut[],
                        const int iFace, const int iVert);

  static void getTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[],
                          const int iFace, const int iVert);

  static void setTSpaceBasic(const SMikkTSpaceContext *pContext,
                             const float fvTangent[], const float fSign,
                             const int iFace, const int iVert);
};
} // namespace Flare
