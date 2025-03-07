#include "CalcTangent.h"

namespace Flare {
CalcTangent::CalcTangent() {
  interface.m_getNumFaces = getNumFaces;
  interface.m_getNumVerticesOfFace = getNumVerticesOfFace;
  interface.m_getPosition = getPosition;
  interface.m_getNormal = getNormal;
  interface.m_getTexCoord = getTexCoord;
  interface.m_setTSpaceBasic = setTSpaceBasic;

  context.m_pInterface = &interface;
}

int CalcTangent::getVertexIndex(const SMikkTSpaceContext *pContext, int iFace,
                                int iVert) {
  const auto *data =
      static_cast<const CalcTangentData *>(pContext->m_pUserData);

  uint32_t index = iFace * getNumVerticesOfFace(pContext, iFace) + iVert;

  return data->indices[index];
}

int CalcTangent::getNumFaces(const SMikkTSpaceContext *pContext) {
  const auto *data =
      static_cast<const CalcTangentData *>(pContext->m_pUserData);

  return data->indices.size() / 3;
}

int CalcTangent::getNumVerticesOfFace(const SMikkTSpaceContext *pContext,
                                      const int iFace) {
  return 3; // only supports triangle
}

void CalcTangent::getPosition(const SMikkTSpaceContext *pContext,
                              float *fvPosOut, const int iFace,
                              const int iVert) {
  const auto *data =
      static_cast<const CalcTangentData *>(pContext->m_pUserData);

  int index = getVertexIndex(pContext, iFace, iVert);

  fvPosOut[0] = data->positions[index].x;
  fvPosOut[1] = data->positions[index].y;
  fvPosOut[2] = data->positions[index].z;
}

void CalcTangent::getNormal(const SMikkTSpaceContext *pContext,
                            float *fvNormOut, const int iFace,
                            const int iVert) {
  const auto *data =
      static_cast<const CalcTangentData *>(pContext->m_pUserData);

  int index = getVertexIndex(pContext, iFace, iVert);

  fvNormOut[0] = data->normals[index].x;
  fvNormOut[1] = data->normals[index].y;
  fvNormOut[2] = data->normals[index].z;
}

void CalcTangent::getTexCoord(const SMikkTSpaceContext *pContext,
                              float *fvTexcOut, const int iFace,
                              const int iVert) {
  const auto *data =
      static_cast<const CalcTangentData *>(pContext->m_pUserData);

  int index = getVertexIndex(pContext, iFace, iVert);

  fvTexcOut[0] = data->uvs[index].s;
  fvTexcOut[1] = data->uvs[index].t;
}

void CalcTangent::setTSpaceBasic(const SMikkTSpaceContext *pContext,
                                 const float *fvTangent, const float fSign,
                                 const int iFace, const int iVert) {
  const auto *data =
      static_cast<const CalcTangentData *>(pContext->m_pUserData);

  int index = getVertexIndex(pContext, iFace, iVert);

  data->tangents[index].x = fvTangent[0];
  data->tangents[index].y = fvTangent[1];
  data->tangents[index].z = fvTangent[2];
  data->tangents[index].w = fSign;
}

void CalcTangent::calculate(CalcTangentData *data) {
  context.m_pUserData = data;

  genTangSpaceDefault(&context);
}
} // namespace Flare