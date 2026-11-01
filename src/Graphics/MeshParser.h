#pragma once

/**
 * @file MeshParser.h
 * @brief Binary parser for Eternity Engine 3D mesh files (.msh).
 *
 * Implements binary parsing for Dragon Nest proprietary version 13 mesh files,
 * extracting submesh geometry, vertex coordinates, normal vectors, UV mapping coordinates,
 * triangle indices, bounding boxes, skeletal bone hierarchy, and attachment dummies.
 */

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace DNItemMaker::Graphics {

/**
 * @brief Two-dimensional floating-point vector for UV texture mapping coordinates.
 */
struct Vector2 {
    float x = 0.0f; ///< U / horizontal texture coordinate.
    float y = 0.0f; ///< V / vertical texture coordinate.
};

/**
 * @brief Three-dimensional floating-point vector for vertex coordinates and normals.
 */
struct Vector3 {
    float x = 0.0f; ///< X coordinate (lateral axis).
    float y = 0.0f; ///< Y coordinate (vertical axis).
    float z = 0.0f; ///< Z coordinate (depth axis).
};

/**
 * @brief Axis-aligned 3D bounding box (AABB) defining spatial mesh boundaries.
 */
struct BoundingBox {
    Vector3 min;    ///< Minimum spatial extent coordinates.
    Vector3 max;    ///< Maximum spatial extent coordinates.

    /** @brief Computes dimensional extent width, height, and depth. */
    Vector3 GetExtents() const { return { max.x - min.x, max.y - min.y, max.z - min.z }; }

    /** @brief Computes geometric center point of the bounding box. */
    Vector3 GetCenter() const { return { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f }; }
};

/**
 * @brief Skeletal vertex skinning influence (4 bone indices and weights per vertex).
 */
struct VertexWeight {
    uint16_t boneIndices[4] = { 0, 0, 0, 0 };
    float weights[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
};

/**
 * @brief Discrete drawable part of a 3D mesh (corresponding to CEtSubMesh in the engine).
 */
struct Submesh {
    std::string name;                   ///< Submesh identifier name.
    std::string parentName;             ///< Parent bone or transform node name.
    int32_t vertexCount = 0;            ///< Total vertex count in this submesh.
    int32_t faceBufferSize = 0;         ///< Raw index buffer count.
    int32_t textureChannels = 0;        ///< Number of UV texture channels.
    bool usesStrip = false;             ///< True if face indices are encoded as a triangle strip.
    bool usesPhysique = false;          ///< True if mesh contains skeletal bone weights and indices.
    bool usesVertexColor = false;       ///< True if vertex color attributes are present.

    std::vector<Vector3> positions;      ///< 3D vertex positions.
    std::vector<Vector3> normals;        ///< Surface normal vectors.
    std::vector<Vector2> uvs;            ///< Primary UV texture mapping coordinates.
    std::vector<uint32_t> triangles;     ///< De-indexed triangle list (3 vertex indices per triangle).
    std::vector<std::string> linkedBones;///< Names of skeletal bones influencing this submesh.
    std::vector<VertexWeight> skinWeights;///< Per-vertex skeletal bone weights and indices.

    /** @brief Returns the total number of triangles in this submesh. */
    int32_t GetTriangleCount() const { return static_cast<int32_t>(triangles.size() / 3); }
};

/**
 * @brief Skeletal bone transform node in the mesh hierarchy.
 */
struct BoneNode {
    std::string name;                   ///< Bone identifier name (e.g. "Bone01", "ValveBiped").
    float invWorldMatrix[16] = {        ///< Inverse bind-pose world transformation matrix.
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
};

/**
 * @brief Attachment dummy node (e.g. weapon mount, particle emitter mount).
 */
struct DummyNode {
    std::string name;                   ///< Dummy identifier (e.g. "#FX_12", "~BoxBone01").
    std::string parentName;             ///< Parent skeletal bone or transform node.
    float matrix[16] = {                ///< Local transformation matrix relative to parent bone.
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
};

/**
 * @brief Fully parsed Eternity Engine 3D mesh definition.
 */
struct ParsedMesh {
    bool valid = false;                 ///< True if binary header and submeshes parsed cleanly.
    std::string errorMessage;           ///< Diagnostic message if parsing failed.
    std::filesystem::path filePath;     ///< Filesystem source path on disk.
    int32_t version = 0;                ///< File format version (typically 13).
    int32_t lodCount = 0;               ///< Level of Detail (LOD) level count.
    bool uvAnimated = false;            ///< True if UV texture animation data is defined.
    int32_t collisionPrimitiveCount = 0;///< Count of collision primitives.
    int32_t dummyCount = 0;             ///< Count of attachment dummy nodes.
    BoundingBox bbox;                   ///< Overall model axis-aligned bounding box.
    std::vector<BoneNode> bones;        ///< Skeletal bone nodes.
    std::vector<Submesh> submeshes;     ///< List of drawable submesh parts.
    std::vector<DummyNode> dummies;     ///< Attachment dummy nodes.

    /** @brief Computes aggregate vertex count across all submeshes. */
    int32_t GetTotalVertices() const {
        int32_t n = 0;
        for (const auto& s : submeshes) n += s.vertexCount;
        return n;
    }

    /** @brief Computes aggregate triangle count across all submeshes. */
    int32_t GetTotalTriangles() const {
        int32_t n = 0;
        for (const auto& s : submeshes) n += s.GetTriangleCount();
        return n;
    }

    /** @brief Formats human-readable diagnostic summary of mesh geometry. */
    std::string GetSummary() const;
};

/**
 * @class MeshParser
 * @brief Static parser for loading Eternity Engine .msh files.
 */
class MeshParser {
public:
    /**
     * @brief Loads and parses a .msh mesh file from disk.
     * @param path Filesystem path to the .msh file.
     * @return ParsedMesh structure containing geometry or error status.
     */
    static ParsedMesh LoadFromFile(const std::filesystem::path& path);

    /**
     * @brief Loads and parses a .msh mesh file directly from a memory buffer.
     * @param data Pointer to raw file bytes.
     * @param size Length of memory buffer in bytes.
     * @param debugPath Optional file identifier for diagnostic logging.
     * @return ParsedMesh structure containing geometry or error status.
     */
    static ParsedMesh LoadFromMemory(const uint8_t* data, size_t size, const std::string& debugPath = "");
};

} // namespace DNItemMaker::Graphics
