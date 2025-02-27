#include "BasicGeometry.h"
#include "CalcTangent.h"

namespace Flare {
    MeshBuffers createCubeMesh(float edgeX, float edgeY, float edgeZ) {
        MeshBuffers meshBuffers;

        float halfEdgeX = edgeX / 2.f;
        float halfEdgeY = edgeY / 2.f;
        float halfEdgeZ = edgeZ / 2.f;

//                3+-------------------+2
//                /|                  /|
//               / |                 / |
//              /  |                /  |
//             /   |               /   |
//            /    |              /    |
//          7+-------------------+6    |
//           |     |             |     |
//           |     |             |     |
//           |     |             |     |
//           |     |             |     |
//           |    0+-------------|-----+1
//           |    /              |    /
//           |   /               |   /
//           |  /                |  /
//           | /                 | /
//           |/                  |/
//          4+-------------------+5
//
//           +y
//           |
//           |
//           +------ +x
//          /
//         /
//        +z

        // Positions of the 8 vertices of the cuboid
        glm::vec4 p[8] = {
                {-halfEdgeX, -halfEdgeY, -halfEdgeZ, 1.0f}, // 0
                {halfEdgeX,  -halfEdgeY, -halfEdgeZ, 1.0f},  // 1
                {halfEdgeX,  halfEdgeY,  -halfEdgeZ, 1.0f},  // 2
                {-halfEdgeX, halfEdgeY,  -halfEdgeZ, 1.0f}, // 3
                {-halfEdgeX, -halfEdgeY, halfEdgeZ,  1.0f}, // 4
                {halfEdgeX,  -halfEdgeY, halfEdgeZ,  1.0f},  // 5
                {halfEdgeX,  halfEdgeY,  halfEdgeZ,  1.0f},  // 6
                {-halfEdgeX, halfEdgeY,  halfEdgeZ,  1.0f}  // 7
        };

        // Normals for each face of the cuboid
        glm::vec4 n[6] = {
                {0.0f,  0.0f,  -1.0f, 1.0f}, // Back face   0
                {0.0f,  0.0f,  1.0f,  1.0f},  // Front face  1
                {-1.0f, 0.0f,  0.0f,  1.0f}, // Left face   2
                {1.0f,  0.0f,  0.0f,  1.0f},  // Right face  3
                {0.0f,  -1.0f, 0.0f,  1.0f}, // Bottom face 4
                {0.0f,  1.0f,  0.0f,  1.0f}   // Top face    5
        };

        // UV coordinates for each face of the cuboid
        glm::vec2 uvs[4] = {
                {0.0f, 0.0f}, // Bottom left  0
                {1.0f, 0.0f}, // Bottom right 1
                {1.0f, 1.0f}, // Top right    2
                {0.0f, 1.0f}  // Top left     3
        };

        meshBuffers.positions = {
                p[1], p[0], p[3], p[2],
                p[4], p[5], p[6], p[7],
                p[0], p[4], p[7], p[3],
                p[5], p[1], p[2], p[6],
                p[0], p[1], p[5], p[4],
                p[7], p[6], p[2], p[3],
        };

        meshBuffers.normals = {
                n[0], n[0], n[0], n[0],
                n[1], n[1], n[1], n[1],
                n[2], n[2], n[2], n[2],
                n[3], n[3], n[3], n[3],
                n[4], n[4], n[4], n[4],
                n[5], n[5], n[5], n[5],
        };

        meshBuffers.uvs = {
                uvs[0], uvs[1], uvs[2], uvs[3],
                uvs[0], uvs[1], uvs[2], uvs[3],
                uvs[0], uvs[1], uvs[2], uvs[3],
                uvs[0], uvs[1], uvs[2], uvs[3],
                uvs[0], uvs[1], uvs[2], uvs[3],
                uvs[0], uvs[1], uvs[2], uvs[3],
        };

        assert(meshBuffers.positions.size() == meshBuffers.normals.size() &&
               meshBuffers.positions.size() == meshBuffers.uvs.size());

        meshBuffers.tangents.resize(meshBuffers.positions.size());

        // Indices, in counterclockwise winding
        meshBuffers.indices = {
                0, 1, 2, 0, 2, 3,       // back face
                4, 5, 6, 4, 6, 7,       // front face
                8, 9, 10, 8, 10, 11,    // left face
                12, 13, 14, 12, 14, 15, // right face
                16, 17, 18, 16, 18, 19, // bottom face
                20, 21, 22, 20, 22, 23, // top face
        };

        CalcTangent calcTangent;
        CalcTangentData calcTangentData = {
                .indices = meshBuffers.indices,
                .positions = meshBuffers.positions,
                .normals = meshBuffers.normals,
                .uvs = meshBuffers.uvs,
                .tangents = meshBuffers.tangents,
        };
        calcTangent.calculate(&calcTangentData);

        return meshBuffers;
    }
}