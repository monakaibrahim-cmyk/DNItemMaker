#include "Graphics/AniParser.h"
#include "Engine/WorkspaceEngine.h"
#include <Common/Logs/Logs.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace DNItemMaker::Graphics {

namespace {
constexpr const char* kAniMagic = "Eternity Engine Ani File";
constexpr size_t kHeaderPaddedSize = 1024;
constexpr size_t kNameSize = 256;

inline bool EqualIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline Vector3 LerpVec3(const Vector3& a, const Vector3& b, float t) noexcept {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}
} // namespace

Quat Quat::Slerp(const Quat& q1, const Quat& q2In, float t) noexcept {
    Quat q2 = q2In;
    float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;

    if (dot < 0.0f) {
        q2.x = -q2.x;
        q2.y = -q2.y;
        q2.z = -q2.z;
        q2.w = -q2.w;
        dot = -dot;
    }

    if (dot > 0.9995f) {
        Quat res{
            q1.x + (q2.x - q1.x) * t,
            q1.y + (q2.y - q1.y) * t,
            q1.z + (q2.z - q1.z) * t,
            q1.w + (q2.w - q1.w) * t
        };
        const float len = std::sqrt(res.x * res.x + res.y * res.y + res.z * res.z + res.w * res.w);
        if (len > 1e-6f) {
            const float inv = 1.0f / len;
            res.x *= inv; res.y *= inv; res.z *= inv; res.w *= inv;
        }
        return res;
    }

    const float theta = std::acos(std::clamp(dot, -1.0f, 1.0f));
    const float sinTheta = std::sin(theta);
    if (std::abs(sinTheta) < 1e-6f) return q1;

    const float w1 = std::sin((1.0f - t) * theta) / sinTheta;
    const float w2 = std::sin(t * theta) / sinTheta;

    return {
        q1.x * w1 + q2.x * w2,
        q1.y * w1 + q2.y * w2,
        q1.z * w1 + q2.z * w2,
        q1.w * w1 + q2.w * w2
    };
}

Matrix4 Matrix4::Identity() noexcept {
    return Matrix4{};
}

Matrix4 Matrix4::Multiply(const Matrix4& a, const Matrix4& b) noexcept {
    Matrix4 r{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r.m[i][j] = a.m[i][0] * b.m[0][j] +
                        a.m[i][1] * b.m[1][j] +
                        a.m[i][2] * b.m[2][j] +
                        a.m[i][3] * b.m[3][j];
        }
    }
    return r;
}

Matrix4 Matrix4::FromScaleRotTrans(const Vector3& s, const Quat& q, const Vector3& t) noexcept {
    Matrix4 r{};
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    const float xx = x * x, yy = y * y, zz = z * z;
    const float xy = x * y, xz = x * z, yz = y * z;
    const float wx = w * x, wy = w * y, wz = w * z;

    r.m[0][0] = (1.0f - 2.0f * (yy + zz)) * s.x;
    r.m[0][1] = (2.0f * (xy + wz)) * s.x;
    r.m[0][2] = (2.0f * (xz - wy)) * s.x;
    r.m[0][3] = 0.0f;

    r.m[1][0] = (2.0f * (xy - wz)) * s.y;
    r.m[1][1] = (1.0f - 2.0f * (xx + zz)) * s.y;
    r.m[1][2] = (2.0f * (yz + wx)) * s.y;
    r.m[1][3] = 0.0f;

    r.m[2][0] = (2.0f * (xz + wy)) * s.z;
    r.m[2][1] = (2.0f * (yz - wx)) * s.z;
    r.m[2][2] = (1.0f - 2.0f * (xx + yy)) * s.z;
    r.m[2][3] = 0.0f;

    r.m[3][0] = t.x;
    r.m[3][1] = t.y;
    r.m[3][2] = t.z;
    r.m[3][3] = 1.0f;

    return r;
}

Vector3 Matrix4::TransformPoint(const Vector3& p) const noexcept {
    return {
        p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0],
        p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1],
        p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2]
    };
}

Vector3 Matrix4::TransformVector(const Vector3& v) const noexcept {
    return {
        v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0],
        v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1],
        v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2]
    };
}

Vector3 BoneTrack::EvaluatePos(float frame) const noexcept {
    if (posKeys.empty()) return defaultPos;
    if (posKeys.size() == 1 || frame <= posKeys.front().time) return posKeys.front().pos;
    if (frame >= posKeys.back().time) return posKeys.back().pos;

    // Binary search for key pair
    auto it = std::upper_bound(posKeys.begin(), posKeys.end(), frame,
                               [](float f, const PositionKey& k) { return f < k.time; });
    if (it == posKeys.begin()) return posKeys.front().pos;
    if (it == posKeys.end()) return posKeys.back().pos;

    const auto& k0 = *(it - 1);
    const auto& k1 = *it;
    const float dt = static_cast<float>(k1.time - k0.time);
    const float t = dt > 0.0f ? (frame - k0.time) / dt : 0.0f;
    return LerpVec3(k0.pos, k1.pos, t);
}

Quat BoneTrack::EvaluateRot(float frame) const noexcept {
    if (rotKeys.empty()) return defaultRot;
    if (rotKeys.size() == 1 || frame <= rotKeys.front().time) return rotKeys.front().rot;
    if (frame >= rotKeys.back().time) return rotKeys.back().rot;

    auto it = std::upper_bound(rotKeys.begin(), rotKeys.end(), frame,
                               [](float f, const RotationKey& k) { return f < k.time; });
    if (it == rotKeys.begin()) return rotKeys.front().rot;
    if (it == rotKeys.end()) return rotKeys.back().rot;

    const auto& k0 = *(it - 1);
    const auto& k1 = *it;
    const float dt = static_cast<float>(k1.time - k0.time);
    const float t = dt > 0.0f ? (frame - k0.time) / dt : 0.0f;
    return Quat::Slerp(k0.rot, k1.rot, t);
}

Vector3 BoneTrack::EvaluateScale(float frame) const noexcept {
    if (scaleKeys.empty()) return defaultScale;
    if (scaleKeys.size() == 1 || frame <= scaleKeys.front().time) return scaleKeys.front().scale;
    if (frame >= scaleKeys.back().time) return scaleKeys.back().scale;

    auto it = std::upper_bound(scaleKeys.begin(), scaleKeys.end(), frame,
                               [](float f, const ScaleKey& k) { return f < k.time; });
    if (it == scaleKeys.begin()) return scaleKeys.front().scale;
    if (it == scaleKeys.end()) return scaleKeys.back().scale;

    const auto& k0 = *(it - 1);
    const auto& k1 = *it;
    const float dt = static_cast<float>(k1.time - k0.time);
    const float t = dt > 0.0f ? (frame - k0.time) / dt : 0.0f;
    return LerpVec3(k0.scale, k1.scale, t);
}

int32_t ParsedAnimationSet::FindBoneIndex(const std::string& boneName) const noexcept {
    for (size_t i = 0; i < bones.size(); ++i) {
        if (EqualIgnoreCase(bones[i].name, boneName)) return static_cast<int32_t>(i);
    }
    return -1;
}

int32_t ParsedAnimationSet::FindClipIndex(const std::string& clipName) const noexcept {
    for (size_t i = 0; i < clips.size(); ++i) {
        if (EqualIgnoreCase(clips[i].name, clipName)) return static_cast<int32_t>(i);
    }
    return -1;
}

void ParsedAnimationSet::EvaluatePose(int32_t clipIndex, float frame, std::vector<Matrix4>& outBoneWorldMatrices) const {
    const size_t numBones = bones.size();
    outBoneWorldMatrices.resize(numBones);
    if (numBones == 0) return;

    // Evaluates forward kinematics recursively or topologically
    std::vector<bool> evaluated(numBones, false);

    auto evalBone = [&](auto& self, size_t boneIdx) -> void {
        if (evaluated[boneIdx]) return;

        const auto& b = bones[boneIdx];
        Vector3 pos{ 0.0f, 0.0f, 0.0f };
        Quat rot = Quat::Identity();
        Vector3 scl{ 1.0f, 1.0f, 1.0f };

        if (clipIndex >= 0 && static_cast<size_t>(clipIndex) < b.tracks.size()) {
            const auto& tr = b.tracks[static_cast<size_t>(clipIndex)];
            pos = tr.EvaluatePos(frame);
            rot = tr.EvaluateRot(frame);
            scl = tr.EvaluateScale(frame);
        }

        Matrix4 localMat = Matrix4::FromScaleRotTrans(scl, rot, pos);

        if (b.parentIndex >= 0 && static_cast<size_t>(b.parentIndex) < numBones) {
            self(self, static_cast<size_t>(b.parentIndex));
            outBoneWorldMatrices[boneIdx] = Matrix4::Multiply(localMat, outBoneWorldMatrices[static_cast<size_t>(b.parentIndex)]);
        } else {
            outBoneWorldMatrices[boneIdx] = localMat;
        }

        evaluated[boneIdx] = true;
    };

    for (size_t i = 0; i < numBones; ++i) {
        evalBone(evalBone, i);
    }
}

namespace {
class Cursor {
public:
    Cursor(const uint8_t* p, size_t n) : m_p(p), m_end(p + n) {}
    bool Ok() const noexcept { return m_ok && m_p <= m_end; }
    size_t Position(const uint8_t* base) const noexcept { return static_cast<size_t>(m_p - base); }

    void Skip(size_t n) noexcept {
        if (m_p + n > m_end) { m_ok = false; return; }
        m_p += n;
    }

    template <typename T>
    T Read() noexcept {
        T val{};
        if (m_p + sizeof(T) > m_end) { m_ok = false; return val; }
        std::memcpy(&val, m_p, sizeof(T));
        m_p += sizeof(T);
        return val;
    }

    std::string ReadFixedString(size_t len) {
        if (m_p + len > m_end) { m_ok = false; return {}; }
        const char* s = reinterpret_cast<const char*>(m_p);
        size_t actualLen = 0;
        while (actualLen < len && s[actualLen] != '\0') ++actualLen;
        m_p += len;
        return std::string(s, actualLen);
    }

private:
    const uint8_t* m_p = nullptr;
    const uint8_t* m_end = nullptr;
    bool m_ok = true;
};
} // namespace

ParsedAnimationSet AniParser::LoadFromFile(const std::filesystem::path& path) {
    ParsedAnimationSet set;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        set.errorMessage = "Failed to open animation file";
        return set;
    }
    const auto size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(size);
    if (!file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size))) {
        set.errorMessage = "Failed to read animation file";
        return set;
    }
    set = LoadFromMemory(buf.data(), buf.size(), path.string());
    set.filePath = path;
    return set;
}

ParsedAnimationSet AniParser::LoadFromMemory(const uint8_t* data, size_t size, const std::string& debugPath) {
    ParsedAnimationSet set;
    if (size < kHeaderPaddedSize) {
        set.errorMessage = "File too small for animation header";
        return set;
    }

    Cursor cur(data, size);
    std::string magic = cur.ReadFixedString(kNameSize);
    if (magic.find(kAniMagic) == std::string::npos) {
        set.errorMessage = "Not an Eternity Engine animation file";
        return set;
    }

    set.version = cur.Read<int32_t>();
    const int32_t boneCount = cur.Read<int32_t>();
    const int32_t aniCount = cur.Read<int32_t>();

    if (!cur.Ok() || boneCount < 0 || boneCount > 4096 || aniCount < 0 || aniCount > 4096) {
        set.errorMessage = "Corrupt animation header counts";
        return set;
    }

    // Skip remainder of 1024-byte header
    const size_t headerUsed = cur.Position(data);
    if (headerUsed < kHeaderPaddedSize) cur.Skip(kHeaderPaddedSize - headerUsed);

    // Read animation clip names (256 bytes each)
    set.clips.reserve(static_cast<size_t>(aniCount));
    for (int32_t i = 0; i < aniCount; ++i) {
        AnimationClip clip;
        clip.name = cur.ReadFixedString(kNameSize);
        set.clips.push_back(std::move(clip));
    }

    // Read animation lengths
    for (int32_t i = 0; i < aniCount; ++i) {
        set.clips[static_cast<size_t>(i)].length = cur.Read<int32_t>();
    }

    // Read bones
    set.bones.reserve(static_cast<size_t>(boneCount));
    for (int32_t b = 0; b < boneCount; ++b) {
        AniBone bone;
        bone.name = cur.ReadFixedString(kNameSize);
        bone.parentName = cur.ReadFixedString(kNameSize);

        // Skip 512 bytes padding (1024 - 512)
        cur.Skip(512);

        bone.tracks.reserve(static_cast<size_t>(aniCount));
        for (int32_t a = 0; a < aniCount; ++a) {
            BoneTrack track;
            track.defaultPos = { cur.Read<float>(), cur.Read<float>(), cur.Read<float>() };
            track.defaultRot = { cur.Read<float>(), cur.Read<float>(), cur.Read<float>(), cur.Read<float>() };
            track.defaultScale = { cur.Read<float>(), cur.Read<float>(), cur.Read<float>() };

            const int32_t posCount = cur.Read<int32_t>();
            if (posCount > 0 && posCount < 65536) {
                track.posKeys.reserve(static_cast<size_t>(posCount));
                for (int32_t k = 0; k < posCount; ++k) {
                    PositionKey pk;
                    pk.time = cur.Read<int16_t>();
                    pk.pos = { cur.Read<float>(), cur.Read<float>(), cur.Read<float>() };
                    track.posKeys.push_back(pk);
                }
            }

            const int32_t rotCount = cur.Read<int32_t>();
            if (rotCount > 0 && rotCount < 65536) {
                track.rotKeys.reserve(static_cast<size_t>(rotCount));
                for (int32_t k = 0; k < rotCount; ++k) {
                    RotationKey rk;
                    rk.time = cur.Read<int16_t>();
                    const int16_t rx = cur.Read<int16_t>();
                    const int16_t ry = cur.Read<int16_t>();
                    const int16_t rz = cur.Read<int16_t>();
                    const int16_t rw = cur.Read<int16_t>();
                    rk.rot = { rx / 32767.0f, ry / 32767.0f, rz / 32767.0f, rw / 32767.0f };
                    track.rotKeys.push_back(rk);
                }
            }

            const int32_t scaleCount = cur.Read<int32_t>();
            if (scaleCount > 0 && scaleCount < 65536) {
                track.scaleKeys.reserve(static_cast<size_t>(scaleCount));
                for (int32_t k = 0; k < scaleCount; ++k) {
                    ScaleKey sk;
                    sk.time = cur.Read<int16_t>();
                    sk.scale = { cur.Read<float>(), cur.Read<float>(), cur.Read<float>() };
                    track.scaleKeys.push_back(sk);
                }
            }

            bone.tracks.push_back(std::move(track));
        }

        set.bones.push_back(std::move(bone));
    }

    if (!cur.Ok()) {
        set.errorMessage = "Unexpected EOF reading animation data";
        return set;
    }

    // Build bone hierarchy links
    for (size_t i = 0; i < set.bones.size(); ++i) {
        if (!set.bones[i].parentName.empty() && set.bones[i].parentName != "Scene Root") {
            const int32_t pIdx = set.FindBoneIndex(set.bones[i].parentName);
            set.bones[i].parentIndex = pIdx;
            if (pIdx >= 0 && static_cast<size_t>(pIdx) < set.bones.size()) {
                set.bones[static_cast<size_t>(pIdx)].childIndices.push_back(static_cast<int32_t>(i));
            }
        }
    }

    set.valid = true;
    LOG_DEBUG(Channels::graphics, "Parsed animation {} (v{}, {} bones, {} clips)",
              debugPath.empty() ? "<memory>" : debugPath, set.version, set.bones.size(), set.clips.size());
    return set;
}

} // namespace DNItemMaker::Graphics
