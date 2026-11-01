#pragma once

/**
 * @file AniParser.h
 * @brief Binary parser for Eternity Engine skeletal animation files (.ani).
 *
 * Implements binary parsing for Dragon Nest proprietary animation files (version 10/11),
 * extracting bone hierarchies, animation clips, keyframed translations, rotations (quaternions),
 * and scaling factors. Supports hierarchical forward kinematics and spherical linear
 * interpolation (Slerp) for smooth 30/60 FPS animation playback.
 */

#include "Graphics/MeshParser.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace DNItemMaker::Graphics {

/**
 * @brief Unit quaternion for 3D skeletal rotation.
 */
struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    static Quat Identity() noexcept { return { 0.0f, 0.0f, 0.0f, 1.0f }; }

    /** @brief Spherical linear interpolation between two quaternions. */
    static Quat Slerp(const Quat& q1, const Quat& q2, float t) noexcept;
};

/**
 * @brief 4x4 row-major transformation matrix (DirectX / Eternity Engine convention: v * M).
 */
struct Matrix4 {
    float m[4][4] = {
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f }
    };

    static Matrix4 Identity() noexcept;
    static Matrix4 Multiply(const Matrix4& a, const Matrix4& b) noexcept;
    static Matrix4 FromScaleRotTrans(const Vector3& scale, const Quat& rot, const Vector3& trans) noexcept;

    Vector3 TransformPoint(const Vector3& p) const noexcept;
    Vector3 TransformVector(const Vector3& v) const noexcept;
};

/**
 * @brief Keyframed translation key.
 */
struct PositionKey {
    int16_t time = 0;   ///< Keyframe timestamp in frames.
    Vector3 pos;        ///< 3D translation coordinates.
};

/**
 * @brief Keyframed quaternion rotation key.
 */
struct RotationKey {
    int16_t time = 0;   ///< Keyframe timestamp in frames.
    Quat rot;           ///< Unit quaternion unpacked from 16-bit integers.
};

/**
 * @brief Keyframed scaling key.
 */
struct ScaleKey {
    int16_t time = 0;   ///< Keyframe timestamp in frames.
    Vector3 scale;      ///< 3D scaling vector.
};

/**
 * @brief Animation channel track for a single bone across a specific animation clip.
 */
struct BoneTrack {
    Vector3 defaultPos;
    Quat defaultRot;
    Vector3 defaultScale{ 1.0f, 1.0f, 1.0f };
    std::vector<PositionKey> posKeys;
    std::vector<RotationKey> rotKeys;
    std::vector<ScaleKey> scaleKeys;

    Vector3 EvaluatePos(float frame) const noexcept;
    Quat EvaluateRot(float frame) const noexcept;
    Vector3 EvaluateScale(float frame) const noexcept;
};

/**
 * @brief Skeletal bone in the animation hierarchy.
 */
struct AniBone {
    std::string name;
    std::string parentName;
    int32_t parentIndex = -1;
    std::vector<int32_t> childIndices;
    std::vector<BoneTrack> tracks; ///< Tracks indexed by animation clip index.
};

/**
 * @brief Animation clip metadata.
 */
struct AnimationClip {
    std::string name;
    int32_t length = 0; ///< Duration in frames (typically at 30 FPS).
};

/**
 * @brief Parsed Eternity Engine skeletal animation dataset (.ani).
 */
struct ParsedAnimationSet {
    bool valid = false;
    std::string errorMessage;
    std::filesystem::path filePath;
    int32_t version = 0;
    std::vector<AnimationClip> clips;
    std::vector<AniBone> bones;

    /** @brief Locates bone index by name (-1 if not found, case-insensitive). */
    int32_t FindBoneIndex(const std::string& boneName) const noexcept;

    /** @brief Locates animation clip index by name (-1 if not found, case-insensitive). */
    int32_t FindClipIndex(const std::string& clipName) const noexcept;

    /**
     * @brief Evaluates world transformation matrices for all bones at the given animation frame.
     * @param clipIndex Animation clip index.
     * @param frame Current animation time in frames.
     * @param outBoneWorldMatrices Output array of bone matrices sized to bones.size().
     */
    void EvaluatePose(int32_t clipIndex, float frame, std::vector<Matrix4>& outBoneWorldMatrices) const;
};

/**
 * @class AniParser
 * @brief Static loader for Eternity Engine .ani files.
 */
class AniParser {
public:
    /** @brief Loads and parses a .ani file from filesystem path. */
    static ParsedAnimationSet LoadFromFile(const std::filesystem::path& path);

    /** @brief Parses raw in-memory .ani byte buffer. */
    static ParsedAnimationSet LoadFromMemory(const uint8_t* data, size_t size, const std::string& debugPath = {});
};

} // namespace DNItemMaker::Graphics
